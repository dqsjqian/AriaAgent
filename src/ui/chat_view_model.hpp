// AriaAgent — ChatViewModel: bridges the agent engine to aria reactive state.
//
// Every piece of UI-visible state is an aria Property / ObservableList so
// the Qt view binds declaratively. The engine runs on a detached worker
// thread; callbacks are marshalled to the UI thread via QMetaObject::invoke.
#pragma once

#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

#include <QObject>

#include <aria/aria.hpp>

#include "agent/agent.hpp"
#include "agent/model.hpp"
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
    aria::Property<std::string>      streaming_text;   // incremental assistant text
    aria::Property<std::string>      phase_text;       // "thinking…" / "tooling…" / ""
    aria::Property<bool>             busy;

    // called by the view (command target)
    void send(const QString& text);
    void stop();

Q_SIGNALS:
    void finished();
    void errorOccurred(const QString& err);

private:
    void finalize_success();
    void finalize_error(const QString& err);
    void push_user(const std::string& text);
    void push_assistant(const std::string& text);
    void push_tool(const UiToolCall& tc);

    agent::ToolRegistry       registry_;
    agent::AgentEngine        engine_;
    std::shared_ptr<UiMessage>   streaming_row_;
    std::unique_ptr<std::thread> worker_;
    std::atomic<bool>         stop_{false};
    std::atomic<bool>         running_{false};
};
