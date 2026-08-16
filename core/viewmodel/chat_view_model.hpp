// AriaAgent — ChatViewModel: bridges the agent engine to reactive state.
//
// Pure C++ / UI-framework agnostic (follows the AiTools Workbench pattern:
// VMs live in `core`, platform shells in `platform/qt|ios|android`).
//
//   - All state is aria Property / ObservableList / TypedSignal — any view
//     (Qt, UIKit, Android Views, Web) binds declaratively to the same VM.
//   - No QObject, no QString, no QMessageBox: the approval prompt is an
//     injected callback the VIEW provides, so mobile shells can reuse the
//     exact same VM.
//   - Cross-thread marshalling uses aria::runtime::main_dispatcher() (the
//     Qt shell installs a QtDispatcher; iOS/Android shells install theirs).
//
// Multi-session: this VM owns the active conversation log (MessageList) and
// persists it through SessionStore on every completed turn. Switching
// sessions reloads the log; the engine replays it for multi-turn context.
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include <aria/aria.hpp>
#include <aria/binding/view_model.hpp>
#include <aria/detail/typed_signal.hpp>

#include "agent/agent.hpp"
#include "agent/model.hpp"
#include "agent/session_store.hpp"
#include "agent/tool_registry.hpp"

// ── UI message row (what the view renders) ──────────────────────────────────
struct UiMessage {
    agent::Role role;
    std::string author;      // "You" / "Agent" / "Tool"
    std::string text;
    bool        is_tool{false};
    std::string tool_name;
};

// ── Tool call row (tool-chain timeline) ─────────────────────────────────────
struct UiToolCall {
    std::string name;
    std::string args;
    std::string result;
    bool        ok{true};
};

// ── ViewModel (framework-agnostic) ──────────────────────────────────────────
class ChatViewModel : public aria::binding::ViewModel {
public:
    explicit ChatViewModel(agent::ToolRegistry tools);
    ~ChatViewModel() override;

    // aria reactive surface (bound by any view)
    aria::ObservableList<UiMessage>  messages;
    aria::ObservableList<UiToolCall> tool_trace;
    aria::Property<std::string>      streaming_text;
    aria::Property<std::string>      phase_text;
    aria::Property<bool>             busy;

    // ── Signals (multicast; view subscribes) ──────────────────────────────
    aria::detail::TypedSignal<>             finished;
    aria::detail::TypedSignal<std::string>  error_occurred;
    aria::detail::TypedSignal<>             session_changed;

    // ── Session management ─────────────────────────────────────────────────
    std::vector<agent::SessionMeta> sessions() const { return store_.list(); }
    std::string current_session_id() const { return current_id_; }
    std::string current_title() const;

    void new_session();                       // create + switch to a fresh one
    void switch_session(const std::string& id);   // save current, load target
    void delete_session(const std::string& id);   // remove file, switch if needed

    // called by the view (command target)
    void send(const std::string& text);
    void stop();

    // Approval hook — the VIEW injects a UI prompt (QMessageBox / UIAlert /
    // Android dialog). VM never shows UI itself. Returning true allows the
    // dangerous tool; false denies (fail-closed).
    std::function<bool(const std::string& tool, const std::string& args)> approval_ui;

private:
    void finalize_success();
    void finalize_error(const std::string& err);
    void maybe_compact();
    void push_user(const std::string& text);
    void push_assistant(const std::string& text);
    void push_tool(const UiToolCall& tc);
    void persist_current();
    std::string derive_title() const;

    agent::ToolRegistry       registry_;
    agent::AgentEngine        engine_;
    agent::SessionStore       store_;
    std::string               current_id_;
    agent::MessageList        history_;       // full conversation log

    std::shared_ptr<UiMessage>   streaming_row_;
    std::unique_ptr<std::thread> worker_;
    std::atomic<bool>         stop_{false};
    std::atomic<bool>         running_{false};
};
