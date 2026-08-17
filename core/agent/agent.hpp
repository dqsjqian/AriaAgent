// AriaAgent — agent engine: coroutine loop around a provider-agnostic LLM.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "agent/model.hpp"
#include "agent/tool_registry.hpp"
#include "agent/llm_client.hpp"

namespace agent {

// Callbacks the engine fires while running; the UI layer hooks these to
// push state into the reactive ViewModel. All callbacks run on the thread
// that invoked run().
struct AgentCallbacks {
    std::function<void(AgentPhase)>                  on_phase;
    std::function<void(const std::string& delta)>    on_text_delta;
    std::function<void(const ToolCallRecord& rec)>   on_tool_call;
    std::function<void(const std::string& err)>      on_error;
    // Approval gate for dangerous tools (requires_approval). Return true to
    // allow, false to deny (fail-closed). Called on the engine thread —
    // implementations must marshal to the UI and block for the answer.
    std::function<bool(const std::string& tool_name,
                       const std::string& args_summary)> on_approval;
};

// The engine is intentionally UI- and provider-agnostic: it talks to an
// LlmClient interface and emits callbacks. The GUI layer adapts those to
// aria reactive state.
class AgentEngine {
public:
    struct Config {
        int max_tool_rounds{8};        // hard stop against runaway loops
        std::string system_prompt{
            "You are a helpful assistant. You may call tools to answer "
            "questions. Reason step by step and always tell the user what "
            "you found."};
    };

    // client: optional. When null (or empty), a default client is created
    // lazily on first run() — so the UI can start without a valid API key
    // and only surface the config error when the user actually sends.
    explicit AgentEngine(ToolRegistry registry, Config cfg,
                         std::unique_ptr<LlmClient> client = {},
                         std::string workspace_root = {});
    ~AgentEngine();

    // Run one turn against a shared, caller-owned conversation log.
    //
    // messages: in/out — the caller appends the user message, the engine
    // appends the assistant reply (and any tool messages) on success, so a
    // multi-turn conversation naturally accumulates and can be persisted.
    // The first message must be a Role::System message (the system prompt).
    // Returns final text reply. Throws std::runtime_error on API errors.
    std::string run(MessageList& messages, const AgentCallbacks& cb);

    // Convenience: run a fresh one-turn conversation (system + user).
    std::string run(const std::string& user_input,
                    const AgentCallbacks& cb) {
        MessageList m;
        m.push_back({Role::System, cfg_.system_prompt, {}, "", "", false});
        m.push_back({Role::User, user_input, {}, "", "", false});
        return run(m, cb);
    }

    // Summarise a conversation prefix into a compact replacement message.
    // Used by compaction to keep long sessions within the context window.
    // Returns the summary text; throws on API error.
    std::string summarize(const MessageList& prefix);

    // Override the base system prompt while preserving tool-specific guidance.
    void set_system_prompt(std::string prompt);
    const std::string& system_prompt() const { return cfg_.system_prompt; }

    // Update the tool execution boundary. Call only while no run is active.
    void set_workspace(std::string root, int access);
    const std::string& workspace_root() const { return workspace_root_; }
    int workspace_access() const { return workspace_access_; }

    // Drop the cached client so the next request re-reads runtime settings.
    // Call only while no run/summarize operation is active.
    void reload_client();

    const std::vector<ToolCallRecord>& tool_trace() const { return trace_; }

private:
    LlmClient& client();

    ToolRegistry          registry_;
    Config                cfg_;
    std::string           tool_guidance_;
    std::string           workspace_root_;
    int                   workspace_access_{1};
    std::unique_ptr<LlmClient> client_;
    std::vector<ToolCallRecord> trace_;
};

} // namespace agent
