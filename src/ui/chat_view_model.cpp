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

    // Start with the most recent session, or create a fresh one.
    auto existing = store_.list();
    if (!existing.empty()) {
        switch_session(QString::fromStdString(existing.front().id));
    } else {
        new_session();
    }
}

ChatViewModel::~ChatViewModel() {
    stop();
    persist_current();
}

// ── Session management ──────────────────────────────────────────────────────
QString ChatViewModel::current_title() const {
    for (const auto& m : store_.list())
        if (m.id == current_id_) return QString::fromStdString(m.title);
    return {};
}

void ChatViewModel::new_session() {
    stop();
    persist_current();
    messages.clear();
    tool_trace.clear();
    history_.clear();
    current_id_ = store_.create();
    phase_text = "";
    streaming_text = "";
    Q_EMIT sessionChanged();
}

void ChatViewModel::switch_session(const QString& id) {
    if (id.toStdString() == current_id_) return;
    stop();
    persist_current();

    current_id_ = id.toStdString();
    messages.clear();
    tool_trace.clear();
    history_ = store_.load(current_id_);

    // Rebuild the UI list from the persisted log.
    for (const auto& m : history_) {
        if (m.role == agent::Role::User) {
            messages.push_back(std::make_shared<UiMessage>(
                UiMessage{agent::Role::User, "You", m.content, false, ""}));
        } else if (m.role == agent::Role::Tool) {
            messages.push_back(std::make_shared<UiMessage>(
                UiMessage{agent::Role::Tool, "Tool", m.tool_result, true, "tool"}));
        } else if (m.role == agent::Role::Assistant) {
            if (m.tool_calls.empty()) {
                messages.push_back(std::make_shared<UiMessage>(
                    UiMessage{agent::Role::Assistant, "Agent", m.content, false, ""}));
            } else {
                for (const auto& tc : m.tool_calls) {
                    tool_trace.push_back(std::make_shared<UiToolCall>(
                        UiToolCall{tc.name, tc.args, "", true}));
                    messages.push_back(std::make_shared<UiMessage>(
                        UiMessage{agent::Role::Tool, "Tool · " + tc.name,
                                  tc.args, true, tc.name}));
                }
                if (!m.content.empty()) {
                    messages.push_back(std::make_shared<UiMessage>(
                        UiMessage{agent::Role::Assistant, "Agent", m.content, false, ""}));
                }
            }
        }
    }
    phase_text = "";
    streaming_text = "";
    Q_EMIT sessionChanged();
}

void ChatViewModel::delete_session(const QString& id) {
    const std::string sid = id.toStdString();
    if (sid == current_id_) {
        // Can't delete the active session while it's running.
        if (running_) return;
    }
    store_.remove(sid);
    if (sid == current_id_) {
        auto remaining = store_.list();
        if (!remaining.empty()) switch_session(QString::fromStdString(remaining.front().id));
        else new_session();
    } else {
        Q_EMIT sessionChanged();
    }
}

// ── Send / stop ─────────────────────────────────────────────────────────────
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

    // User message goes into the shared log (multi-turn context).
    history_.push_back({agent::Role::User, input, {}, "", "", false});
    messages.push_back(std::make_shared<UiMessage>(
        UiMessage{agent::Role::User, "You", input, false, ""}));

    streaming_row_ = std::make_shared<UiMessage>(
        UiMessage{agent::Role::Assistant, "Agent", "", false, ""});
    messages.push_back(streaming_row_);
    streaming_text = "";

    // Engine runs off the UI thread; callbacks hop back to UI.
    worker_ = std::make_unique<std::thread>([this] {
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

        try {
            engine_.run(history_, cb);   // appends assistant reply to history_
        } catch (const std::exception& ex) {
            Q_EMIT errorOccurred(QString::fromStdString(ex.what()));
            return;
        }
        post_to_ui(this, [this] { Q_EMIT finished(); });
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

// ── Finalize ────────────────────────────────────────────────────────────────
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
    persist_current();
    maybe_compact();
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
    persist_current();
}

void ChatViewModel::persist_current() {
    if (current_id_.empty()) return;
    store_.save(current_id_, history_, derive_title());
}

std::string ChatViewModel::derive_title() const {
    for (const auto& m : history_)
        if (m.role == agent::Role::User && !m.content.empty()) {
            auto t = m.content;
            if (t.size() > 24) t = t.substr(0, 24) + "…";
            return t;
        }
    return {};
}

void ChatViewModel::maybe_compact() {
    // Ported from harness compaction: when the log grows large, replace the
    // old prefix with a model-generated summary. Never split a tool-call /
    // tool-result pair, and never summarise the most recent window.
    constexpr size_t kThreshold = 32;     // messages before considering
    constexpr size_t kKeepTail = 8;       // recent messages always kept verbatim
    if (history_.size() < kThreshold) return;

    // Find the cut index: walk back from the end, stop once we've collected
    // kKeepTail messages and we're at a safe boundary (not a tool result
    // whose call is in the compacted region).
    size_t cut = history_.size() - kKeepTail;
    while (cut > 1) {
        const auto& m = history_[cut];
        if (m.role == agent::Role::Tool) { ++cut; break; }   // keep its call
        break;
    }
    if (cut <= 1) return;   // nothing meaningful to compact

    // Build the prefix to summarise: [system] + [cut-1 messages].
    agent::MessageList prefix;
    prefix.push_back(history_.front());                    // system prompt
    for (size_t i = 1; i < cut; ++i) prefix.push_back(history_[i]);

    QString old_phase = phase_text.get().c_str();
    phase_text = "compacting…";

    worker_ = std::make_unique<std::thread>([this, prefix, cut, old_phase] {
        std::string summary;
        bool ok = false;
        try {
            summary = engine_.summarize(prefix);
            ok = true;
        } catch (const std::exception&) { /* keep history as-is on failure */ }
        post_to_ui(this, [this, prefix, cut, summary, ok, old_phase] {
            phase_text = old_phase.toStdString();
            if (!ok || summary.empty()) return;

            // Replace history_[1..cut) with a single system-compaction note.
            agent::MessageList compacted;
            compacted.push_back(history_.front());
            compacted.push_back({agent::Role::System,
                "[Compacted conversation summary] " + summary, {}, "", "", false});
            for (size_t i = cut; i < history_.size(); ++i)
                compacted.push_back(history_[i]);
            history_ = std::move(compacted);

            // Rebuild UI list.
            messages.clear();
            tool_trace.clear();
            for (const auto& m : history_) {
                if (m.role == agent::Role::User) {
                    messages.push_back(std::make_shared<UiMessage>(
                        UiMessage{agent::Role::User, "You", m.content, false, ""}));
                } else if (m.role == agent::Role::Tool) {
                    messages.push_back(std::make_shared<UiMessage>(
                        UiMessage{agent::Role::Tool, "Tool", m.tool_result, true, "tool"}));
                } else if (m.role == agent::Role::Assistant) {
                    if (m.tool_calls.empty()) {
                        messages.push_back(std::make_shared<UiMessage>(
                            UiMessage{agent::Role::Assistant, "Agent", m.content, false, ""}));
                    } else {
                        for (const auto& tc : m.tool_calls)
                            messages.push_back(std::make_shared<UiMessage>(
                                UiMessage{agent::Role::Tool, "Tool · " + tc.name,
                                          tc.args, true, tc.name}));
                        if (!m.content.empty())
                            messages.push_back(std::make_shared<UiMessage>(
                                UiMessage{agent::Role::Assistant, "Agent", m.content, false, ""}));
                    }
                }
            }
            persist_current();
        });
    });
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
