// AriaAgent — ChatViewModel implementation.
#include "ui/chat_view_model.hpp"

#include <QMetaObject>
#include <QThread>

namespace {

// Marshal a std::function to the UI thread; if already there, run inline.
template <typename F>
void post_to_ui(QObject* ctx, F&& f) {
    if (QThread::currentThread() == ctx->thread()) {
        f();
    } else {
        QMetaObject::invokeMethod(ctx, [f = std::forward<F>(f)]() mutable { f(); },
                                  Qt::QueuedConnection);
    }
}

} // namespace

ChatViewModel::ChatViewModel(agent::ToolRegistry tools, QObject* parent)
    : QObject(parent),
      registry_(std::move(tools)),
      engine_(registry_, agent::AgentEngine::Config{}) {
    streaming_text = "";
    phase_text = "";
    busy = false;
    connect(this, &ChatViewModel::finished, this, &ChatViewModel::finalize_success);
    connect(this, &ChatViewModel::errorOccurred, this, &ChatViewModel::finalize_error);
}
ChatViewModel::~ChatViewModel() {
    stop();
}

void ChatViewModel::send(const QString& text) {
    const std::string input = text.toStdString();
    if (input.empty() || running_) return;

    // Settings dialog writes these env vars on save; pick up any updates.
    if (const char* p = std::getenv("ARIA_LLM_SYSTEM_PROMPT"); p && *p) {
        engine_.set_system_prompt(p);
    }

    running_ = true;
    busy = true;
    phase_text = "thinking…";
    messages.push_back(std::make_shared<UiMessage>(
        UiMessage{agent::Role::User, "You", input, false, ""}));
    // Reserve a streaming assistant row; keep the shared_ptr for live updates.
    streaming_row_ = std::make_shared<UiMessage>(
        UiMessage{agent::Role::Assistant, "Agent", "", false, ""});
    messages.push_back(streaming_row_);
    streaming_text = "";

    // Engine runs off the UI thread; callbacks hop back to UI.
    worker_ = std::make_unique<std::thread>([this, input] {
        agent::AgentCallbacks cb;
        cb.on_text_delta = [this](const std::string& d) {
            post_to_ui(this, [this, d] {
                if (stop_) return;
                streaming_text = streaming_text.get() + d;
                if (streaming_row_) {
                    streaming_row_->text = streaming_text.get();
                    messages.replace_at(messages.size() - 1, streaming_row_);
                }
            });
        };
        cb.on_tool_call = [this](const agent::ToolCallRecord& rec) {
            post_to_ui(this, [this, rec] {
                if (stop_) return;
                phase_text = "tooling… (" + rec.name + ")";
                UiToolCall tc{rec.name, rec.args, rec.result, rec.succeeded};
                push_tool(tc);
            });
        };
        cb.on_phase = [this](agent::AgentPhase p) {
            post_to_ui(this, [this, p] {
                if (stop_) return;
                switch (p) {
                    case agent::AgentPhase::Thinking:  phase_text = "thinking…"; break;
                    case agent::AgentPhase::Tooling:   phase_text = "tooling…";  break;
                    default:                           phase_text = "";          break;
                }
            });
        };
        cb.on_error = [this](const std::string& e) {
            post_to_ui(this, [this, e] { Q_EMIT errorOccurred(QString::fromStdString(e)); });
        };

        std::string reply;
        try {
            reply = engine_.run(input, cb);
        } catch (const std::exception& ex) {
            Q_EMIT errorOccurred(QString::fromStdString(ex.what()));
            return;
        }
        post_to_ui(this, [this, reply] { Q_EMIT finished(); });
        (void)reply;
    });
}

void ChatViewModel::stop() {
    stop_ = true;
    if (worker_ && worker_->joinable()) {
        worker_->join();
    }
    worker_.reset();
    stop_ = false;
    running_ = false;
    busy = false;
}

void ChatViewModel::finalize_success() {
    if (streaming_row_) {
        streaming_row_->text = streaming_text.get();
        messages.replace_at(messages.size() - 1, streaming_row_);
        streaming_row_.reset();
    }
    running_ = false;
    busy = false;
    phase_text = "";
    streaming_text = "";
}

void ChatViewModel::finalize_error(const QString& err) {
    if (streaming_row_) {
        streaming_row_->text = "⚠ Error: " + err.toStdString();
        messages.replace_at(messages.size() - 1, streaming_row_);
        streaming_row_.reset();
    } else {
        push_assistant("⚠ Error: " + err.toStdString());
    }
    running_ = false;
    busy = false;
    phase_text = "";
    streaming_text = "";
}

void ChatViewModel::push_user(const std::string& text) {
    messages.push_back(std::make_shared<UiMessage>(
        UiMessage{agent::Role::User, "You", text, false, ""}));
}

void ChatViewModel::push_assistant(const std::string& text) {
    messages.push_back(std::make_shared<UiMessage>(
        UiMessage{agent::Role::Assistant, "Agent", text, false, ""}));
}

void ChatViewModel::push_tool(const UiToolCall& tc) {
    tool_trace.push_back(std::make_shared<UiToolCall>(tc));
    messages.push_back(std::make_shared<UiMessage>(
        UiMessage{agent::Role::Tool, "Tool · " + tc.name,
                  tc.args + "  →  " + tc.result, true, tc.name}));
}
