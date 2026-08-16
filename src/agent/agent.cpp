// AriaAgent — agent engine implementation (synchronous loop).
//
// Design note: the engine itself is a plain blocking function so it can run
// on any worker thread. The GUI layer runs it inside a std::thread and
// marshals callbacks to the UI thread; nothing in this file touches Qt or
// aria reactive state directly.
#include "agent/agent.hpp"

#include <chrono>
#include <stdexcept>

namespace agent {

using json = nlohmann::json;

AgentEngine::AgentEngine(ToolRegistry registry, Config cfg,
                         std::unique_ptr<LlmClient> client)
    : registry_(std::move(registry)),
      cfg_(std::move(cfg)),
      client_(std::move(client)) {}

AgentEngine::~AgentEngine() = default;

LlmClient& AgentEngine::client() {
    if (!client_) {
        client_ = create_llm_client();
    }
    return *client_;
}

std::string AgentEngine::run(const std::string& user_input,
                             const AgentCallbacks& cb) {
    trace_.clear();

    MessageList messages;
    messages.push_back({Role::System, cfg_.system_prompt, {}, "", "", false});
    messages.push_back({Role::User, user_input, {}, "", "", false});

    json tools = registry_.schema();

    if (cb.on_phase) cb.on_phase(AgentPhase::Thinking);

    for (int round = 0; round < cfg_.max_tool_rounds; ++round) {
        // ── Ask the model (streaming text; tool_calls may also arrive) ──
        std::string assistant_text;
        std::vector<ToolCallInfo> pending_tool_calls;

        if (cb.on_phase) cb.on_phase(AgentPhase::Streaming);

        client().complete_stream(messages, tools, [&](const StreamEvent& ev) {
            if (!ev.delta.empty()) {
                assistant_text += ev.delta;
                if (cb.on_text_delta) cb.on_text_delta(ev.delta);
            }
            for (const auto& tc : ev.tool_calls) {
                pending_tool_calls.push_back(tc);
            }
        });

        // ── If the model wants tools: execute them, feed results back ──
        if (!pending_tool_calls.empty()) {
            if (cb.on_phase) cb.on_phase(AgentPhase::Tooling);

            // Merge all streamed tool_calls into one assistant message.
            ChatMessage assistant_msg;
            assistant_msg.role = Role::Assistant;
            assistant_msg.content = assistant_text;
            for (auto& tc : pending_tool_calls) {
                if (tc.id.empty())
                    tc.id = "call_" + std::to_string(round) + "_" + tc.name;
                assistant_msg.tool_calls.push_back(std::move(tc));
            }
            messages.push_back(std::move(assistant_msg));

            ToolContext ctx;
            for (const auto& tc : assistant_msg.tool_calls) {
                ToolCallRecord rec;
                rec.id = tc.id;
                rec.name = tc.name;
                rec.args = tc.args.empty() ? "{}" : tc.args;

                auto t0 = std::chrono::steady_clock::now();
                std::string result_text;
                try {
                    json args = json::parse(tc.args.empty() ? "{}" : tc.args);
                    auto r = registry_.run(tc.name, args, ctx);
                    if (r) {
                        result_text = r->dump();
                    } else {
                        result_text = "error: unknown tool '" + tc.name + "'";
                        rec.succeeded = false;
                    }
                } catch (const std::exception& e) {
                    result_text = std::string("error: ") + e.what();
                    rec.succeeded = false;
                }
                auto t1 = std::chrono::steady_clock::now();
                rec.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
                rec.result = result_text;
                trace_.push_back(rec);
                if (cb.on_tool_call) cb.on_tool_call(rec);

                // Tool result message (role=tool) referencing the call id.
                messages.push_back({Role::Tool, result_text, {}, tc.id, result_text, false});
            }
            continue;   // loop: send updated conversation back to the model
        }

        // ── No tool calls → done ──
        if (cb.on_phase) cb.on_phase(AgentPhase::Done);
        return assistant_text;
    }

    // Tool-loop limit reached without a final answer.
    std::string msg = "Reached max tool rounds (" +
                      std::to_string(cfg_.max_tool_rounds) + ")";
    if (cb.on_phase) cb.on_phase(AgentPhase::Error);
    if (cb.on_error) cb.on_error(msg);
    throw std::runtime_error(msg);
}

} // namespace agent
