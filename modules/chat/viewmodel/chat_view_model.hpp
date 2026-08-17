// AriaAgent — ChatViewModel: the conversation feature module.
//
// Pure C++ / UI-framework agnostic. Depends ONLY on services (SessionStore,
// ToolTraceStore, ToolRegistry) obtained at construction — it never
// references sibling ViewModels, so every module is a swappable plugin.
//
// All state is aria Property / ObservableList / TypedSignal; the approval
// prompt is an injected callback the VIEW provides; cross-thread marshalling
// goes through aria::runtime::main_dispatcher().
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include <aria/aria.hpp>
#include <aria/detail/typed_signal.hpp>
#include <aria/subscription.hpp>

#include "module_api/BaseVm.h"
#include "ui_types.hpp"

#include "agent/agent.hpp"
#include "agent/model.hpp"
#include "agent/session_store.hpp"
#include "agent/tool_registry.hpp"

#include "trajectory/services/tool_trace_store.hpp"

class ChatViewModel : public BaseVm {
public:
    ChatViewModel(agent::ToolRegistry& registry,
                  agent::SessionStore& sessions,
                  agent::ToolTraceStore& trace);
    ~ChatViewModel() override;

    // ── Reactive surface (bound by any view) ───────────────────────────────
    aria::ObservableList<UiMessage>  messages;
    aria::Property<std::string>      streaming_text;
    aria::Property<std::string>      phase_text;
    aria::Property<bool>             busy;

    aria::detail::TypedSignal<>             finished;
    aria::detail::TypedSignal<std::string>  error_occurred;

    // ── Actions (called by the View) ───────────────────────────────────────
    void send(const std::string& text);
    void stop();

    /// Recreate the lazy LLM client before the next request.
    void reload_model_settings();

    /// Update the root and access mode used by subsequent tool calls.
    bool set_workspace(const std::string& root, int access);

    /// Rebuild the UI list from the current session log (invoked when the
    /// SessionStore switches/creates/deletes sessions).
    void reload_messages();

    // Approval hook — the VIEW injects a UI prompt. VM never shows UI.
    std::function<bool(const std::string& tool, const std::string& args)> approval_ui;

private:
    void finalize_success();
    void finalize_error(const std::string& err);
    void maybe_compact();
    void push_user(const std::string& text);
    void push_assistant(const std::string& text);
    void push_tool(const agent::ToolCallRecord& rec);

    agent::ToolRegistry&   registry_;
    agent::AgentEngine     engine_;
    agent::SessionStore&   sessions_;
    agent::ToolTraceStore& trace_;

    aria::Subscription     session_sub_;

    std::shared_ptr<UiMessage>   streaming_row_;
    std::unique_ptr<std::thread> worker_;
    std::atomic<bool>         stop_{false};
    std::atomic<bool>         running_{false};
    std::atomic<bool>         compacting_{false};
    std::atomic<bool>         model_settings_dirty_{false};
    std::atomic<bool>         skill_loaded_this_run_{false};
};
