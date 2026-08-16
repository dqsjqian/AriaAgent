// AriaAgent — ChatViewModel: bridges the agent engine to aria reactive state.
//
// Every piece of UI-visible state is an aria Property / ObservableList so
// the Qt view binds declaratively. The engine runs on a detached worker
// thread; callbacks are marshalled to the UI thread via QMetaObject::invoke.
//
// Multi-session: this VM owns the active conversation log (MessageList) and
// persists it through SessionStore on every completed turn. Switching
// sessions reloads the log; the engine replays it for multi-turn context.
#pragma once

#include <memory>
#include <thread>
#include <atomic>

#include <QObject>

#include <aria/aria.hpp>

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

// ── ViewModel ───────────────────────────────────────────────────────────────
class ChatViewModel : public QObject {
    Q_OBJECT
public:
    explicit ChatViewModel(agent::ToolRegistry tools, QObject* parent = nullptr);
    ~ChatViewModel() override;

    // aria reactive surface (bound by the view)
    aria::ObservableList<UiMessage>  messages;
    aria::ObservableList<UiToolCall> tool_trace;
    aria::Property<std::string>      streaming_text;
    aria::Property<std::string>      phase_text;
    aria::Property<bool>             busy;

    // ── Session management ─────────────────────────────────────────────────
    std::vector<agent::SessionMeta> sessions() const { return store_.list(); }
    std::string current_session_id() const { return current_id_; }
    QString current_title() const;

    void new_session();                       // create + switch to a fresh one
    void switch_session(const QString& id);   // save current, load target
    void delete_session(const QString& id);   // remove file, switch if needed

    // called by the view (command target)
    void send(const QString& text);
    void stop();

Q_SIGNALS:
    void finished();
    void errorOccurred(const QString& err);
    void sessionChanged();       // current session id/title changed

private:
    void finalize_success();
    void finalize_error(const QString& err);
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
