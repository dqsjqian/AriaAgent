// AriaAgent — ChatViewModel implementation (framework-agnostic core).
#include "chat/viewmodel/chat_view_model.hpp"

#include "i18n/I18n.h"

#include <aria/runtime/dispatcher.hpp>

#include <cstdlib>

namespace {

// Marshal a closure to the main (UI) thread via aria's dispatcher; the
// Qt shell installs a QtDispatcher at startup, iOS/Android install their
// own — the VM stays platform-agnostic.
template <typename F>
void post_to_ui(F&& f) {
    auto& d = aria::runtime::main_dispatcher();
    if (d.is_main_thread()) {
        f();
    } else {
        d.post(std::forward<F>(f));
    }
}

} // namespace

ChatViewModel::ChatViewModel(agent::ToolRegistry& registry,
                             agent::SessionStore& sessions,
                             agent::ToolTraceStore& trace)
    : registry_(registry),
      engine_(registry_, agent::AgentEngine::Config{}),
      sessions_(sessions),
      trace_(trace) {
    streaming_text = "";
    phase_text = "";
    busy = false;

    // When the session store changes (create/switch/delete) the UI list must
    // be rebuilt from the (new) current log.
    session_sub_ = sessions_.session_changed.connect(
        [this] { reload_messages(); });
    reload_messages();
}

ChatViewModel::~ChatViewModel() {
    stop();
    sessions_.persist_current();
}

// ── Reload from the current session log ─────────────────────────────────────
void ChatViewModel::reload_messages() {
    messages.clear();
    trace_.clear();
    const auto& history = sessions_.current_history();
    for (const auto& m : history) {
        if (m.role == agent::Role::User) {
            push_user(m.content);
        } else if (m.role == agent::Role::Tool) {
            messages.push_back(std::make_shared<UiMessage>(
                UiMessage{agent::Role::Tool, "Tool",
                          agent::i18n::str("msg_tool"), m.tool_result, true, "tool"}));
        } else if (m.role == agent::Role::Assistant) {
            if (m.tool_calls.empty()) {
                push_assistant(m.content);
            } else {
                for (const auto& tc : m.tool_calls) {
                    trace_.trace().push_back(std::make_shared<UiToolCall>(
                        UiToolCall{tc.name, tc.args, "", true}));
                    messages.push_back(std::make_shared<UiMessage>(
                        UiMessage{agent::Role::Tool, "Tool",
                                  agent::i18n::str("msg_tool_prefix") + tc.name,
                                  tc.args, true, tc.name}));
                }
                if (!m.content.empty()) push_assistant(m.content);
            }
        }
    }
    phase_text = "";
    streaming_text = "";
}

// ── Send / stop ─────────────────────────────────────────────────────────────
void ChatViewModel::send(const std::string& input) {
    if (input.empty() || running_) return;

    // Settings module writes these env vars on save; pick up any updates.
    // When unset, fall back to the localized default so the model gets a
    // prompt in the user's language even before they open Settings.
    if (const char* p = std::getenv("ARIA_LLM_SYSTEM_PROMPT"); p && *p) {
        engine_.set_system_prompt(p);
    } else {
        engine_.set_system_prompt(agent::i18n::str("default_system_prompt"));
    }

    running_ = true;
    busy = true;
    phase_text = agent::i18n::str("phase_thinking");

    // User message goes into the shared log (multi-turn context).
    auto& history = sessions_.current_history();
    history.push_back({agent::Role::User, input, {}, "", "", false});
    push_user(input);

    streaming_row_ = std::make_shared<UiMessage>(
        UiMessage{agent::Role::Assistant, "Agent",
                  agent::i18n::str("msg_agent"), "", false, ""});
    messages.push_back(streaming_row_);
    streaming_text = "";

    // Engine runs off the UI thread; callbacks hop back to UI via dispatcher.
    worker_ = std::make_unique<std::thread>([this] {
        agent::AgentCallbacks cb;
        cb.on_text_delta = [this](const std::string& d) {
            post_to_ui([this, d] {
                if (stop_) return;
                streaming_text = streaming_text.get() + d;
                if (streaming_row_) {
                    streaming_row_->text = streaming_text.get();
                    messages.replace_at(messages.size() - 1, streaming_row_);
                }
            });
        };
        cb.on_tool_call = [this](const agent::ToolCallRecord& rec) {
            post_to_ui([this, rec] {
                if (stop_) return;
                phase_text = agent::i18n::str("phase_tooling") + " (" + rec.name + ")";
                push_tool(rec);
            });
        };
        cb.on_phase = [this](agent::AgentPhase p) {
            post_to_ui([this, p] {
                if (stop_) return;
                switch (p) {
                    case agent::AgentPhase::Thinking:  phase_text = agent::i18n::str("phase_thinking"); break;
                    case agent::AgentPhase::Tooling:   phase_text = agent::i18n::str("phase_tooling");  break;
                    default:                           phase_text = "";          break;
                }
            });
        };
        cb.on_approval = [this](const std::string& tool,
                                const std::string& args) {
            // Full Access (level 2) skips the prompt entirely.
            if (const char* mode = std::getenv("ARIA_WORKSPACE_WRITE");
                mode && *mode == '2') {
                return true;
            }
            // The VIEW owns the approval prompt; VM just asks. If no UI is
            // injected (headless/mobile shell w/o prompt yet) → fail closed.
            if (!approval_ui) return false;
            bool approved = false;
            post_to_ui([&] { approved = approval_ui(tool, args); });
            return approved;
        };
        cb.on_error = [this](const std::string& e) {
            post_to_ui([this, e] {
                finalize_error(e);
                error_occurred.emit(e);
            });
        };

        try {
            engine_.run(sessions_.current_history(), cb);  // appends reply
        } catch (const std::exception& ex) {
            post_to_ui([this, msg = std::string(ex.what())] {
                finalize_error(msg);
                error_occurred.emit(msg);
            });
            return;
        }
        post_to_ui([this] {
            finalize_success();
            finished.emit();
        });
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
    sessions_.persist_current();
    maybe_compact();
}

void ChatViewModel::finalize_error(const std::string& err) {
    if (streaming_row_) {
        streaming_row_->text = agent::i18n::str("msg_error_prefix") + err;
        messages.replace_at(messages.size() - 1, streaming_row_);
        streaming_row_.reset();
    } else {
        push_assistant(agent::i18n::str("msg_error_prefix") + err);
    }
    running_ = false;
    busy = false;
    phase_text = agent::i18n::str("phase_error");
    streaming_text = "";
    sessions_.persist_current();
}

void ChatViewModel::maybe_compact() {
    // Ported from harness compaction: when the log grows large, replace the
    // old prefix with a model-generated summary. Never split a tool-call /
    // tool-result pair, and never summarise the most recent window.
    constexpr size_t kThreshold = 32;     // messages before considering
    constexpr size_t kKeepTail = 8;       // recent messages always kept verbatim
    auto& history = sessions_.current_history();
    if (history.size() < kThreshold) return;

    size_t cut = history.size() - kKeepTail;
    while (cut > 1) {
        const auto& m = history[cut];
        if (m.role == agent::Role::Tool) { ++cut; break; }   // keep its call
        break;
    }
    if (cut <= 1) return;   // nothing meaningful to compact

    // Build the prefix to summarise: [system] + [cut-1 messages].
    agent::MessageList prefix;
    prefix.push_back(history.front());                    // system prompt
    for (size_t i = 1; i < cut; ++i) prefix.push_back(history[i]);

    const std::string old_phase = phase_text.get();
    phase_text = agent::i18n::str("phase_compacting");

    worker_ = std::make_unique<std::thread>([this, prefix, cut, old_phase] {
        std::string summary;
        bool ok = false;
        try {
            summary = engine_.summarize(prefix);
            ok = true;
        } catch (const std::exception&) { /* keep history as-is on failure */ }
        post_to_ui([this, prefix, cut, summary, ok, old_phase] {
            phase_text = old_phase;
            if (!ok || summary.empty()) return;

            // Replace history_[1..cut) with a single system-compaction note.
            auto& history = sessions_.current_history();
            agent::MessageList compacted;
            compacted.push_back(history.front());
            compacted.push_back({agent::Role::System,
                agent::i18n::str("msg_compacted") + summary, {}, "", "", false});
            for (size_t i = cut; i < history.size(); ++i)
                compacted.push_back(history[i]);
            history = std::move(compacted);

            reload_messages();
            sessions_.persist_current();
        });
    });
}

void ChatViewModel::push_user(const std::string& text) {
    messages.push_back(std::make_shared<UiMessage>(
        UiMessage{agent::Role::User, "You", agent::i18n::str("msg_user"),
                  text, false, ""}));
}

void ChatViewModel::push_assistant(const std::string& text) {
    messages.push_back(std::make_shared<UiMessage>(
        UiMessage{agent::Role::Assistant, "Agent", agent::i18n::str("msg_agent"),
                  text, false, ""}));
}

void ChatViewModel::push_tool(const agent::ToolCallRecord& rec) {
    trace_.add(rec);
    messages.push_back(std::make_shared<UiMessage>(
        UiMessage{agent::Role::Tool, "Tool",
                  agent::i18n::str("msg_tool_prefix") + rec.name,
                  rec.args + "  →  " + rec.result, true, rec.name}));
}
