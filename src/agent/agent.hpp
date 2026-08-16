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

    // client: ownership transferred to the engine. If null, a default
    // client is created from environment configuration.
    explicit AgentEngine(ToolRegistry registry, Config cfg,
                         std::unique_ptr<LlmClient> client = {});

    // Synchronous run: blocks the calling thread until done.
    // Returns final text reply. Throws std::runtime_error on API errors.
    std::string run(const std::string& user_input,
                    const AgentCallbacks& cb);

    const std::vector<ToolCallRecord>& tool_trace() const { return trace_; }

private:
    ToolRegistry          registry_;
    Config                cfg_;
    std::unique_ptr<LlmClient> client_;
    std::vector<ToolCallRecord> trace_;
};

} // namespace agent
