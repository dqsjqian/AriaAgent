// AriaAgent — agent engine implementation (synchronous loop).
//
// Design note: the engine itself is a plain blocking function so it can run
// on any worker thread. The GUI layer runs it inside a std::thread and
// marshals callbacks to the UI thread; nothing in this file touches Qt or
// aria reactive state directly.
#include "agent/agent.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <thread>
#include <utility>

namespace agent {

using json = nlohmann::json;

namespace {

void remove_incomplete_tool_groups(MessageList& messages) {
    MessageList repaired;
    repaired.reserve(messages.size());
    for (size_t i = 0; i < messages.size();) {
        const auto& message = messages[i];
        if (message.role == Role::Tool) {
            ++i; // orphaned tool output
            continue;
        }
        if (message.role != Role::Assistant || message.tool_calls.empty()) {
            repaired.push_back(message);
            ++i;
            continue;
        }

        std::vector<std::string> expected;
        bool valid = true;
        for (const auto& call : message.tool_calls) {
            if (call.id.empty() || call.name.empty() ||
                std::find(expected.begin(), expected.end(), call.id) != expected.end()) {
                valid = false;
                break;
            }
            expected.push_back(call.id);
        }

        size_t end = i + 1;
        std::vector<std::string> outputs;
        while (end < messages.size() && messages[end].role == Role::Tool) {
            const auto& id = messages[end].tool_call_id;
            if (id.empty() || std::find(outputs.begin(), outputs.end(), id) != outputs.end()) {
                valid = false;
            } else {
                outputs.push_back(id);
            }
            ++end;
        }
        for (const auto& id : expected) {
            if (std::find(outputs.begin(), outputs.end(), id) == outputs.end()) valid = false;
        }
        if (outputs.size() != expected.size()) valid = false;

        if (valid) {
            repaired.insert(repaired.end(), messages.begin() + static_cast<std::ptrdiff_t>(i),
                            messages.begin() + static_cast<std::ptrdiff_t>(end));
        }
        i = end;
    }
    messages = std::move(repaired);
}

} // namespace

AgentEngine::AgentEngine(ToolRegistry registry, Config cfg,
                         std::unique_ptr<LlmClient> client,
                         std::string workspace_root)
    : registry_(std::move(registry)),
      cfg_(std::move(cfg)),
      workspace_root_(std::move(workspace_root)),
      client_(std::move(client)) {
    if (workspace_root_.empty()) {
        if (const char* configured = std::getenv("ARIA_WORKSPACE_ROOT"); configured && *configured) {
            workspace_root_ = configured;
        } else {
            std::error_code ec;
            workspace_root_ = std::filesystem::current_path(ec).string();
        }
    }
    std::error_code ec;
    const auto canonical = std::filesystem::weakly_canonical(workspace_root_, ec);
    if (!ec) workspace_root_ = canonical.string();

    for (const auto& guidance : registry_.prompt_guidance()) {
        if (!tool_guidance_.empty()) tool_guidance_ += "\n\n";
        tool_guidance_ += guidance;
    }
    set_system_prompt(std::move(cfg_.system_prompt));
}

AgentEngine::~AgentEngine() = default;

LlmClient& AgentEngine::client() {
    if (!client_) {
        client_ = create_llm_client();
    }
    return *client_;
}

void AgentEngine::set_system_prompt(std::string prompt) {
    cfg_.system_prompt = std::move(prompt);
    if (!tool_guidance_.empty()) {
        if (!cfg_.system_prompt.empty()) cfg_.system_prompt += "\n\n";
        cfg_.system_prompt += tool_guidance_;
    }
}

void AgentEngine::set_workspace(std::string root, int access) {
    std::error_code ec;
    const auto canonical = std::filesystem::canonical(root, ec);
    if (ec || !std::filesystem::is_directory(canonical, ec) || ec) {
        throw std::invalid_argument("workspace directory does not exist");
    }
    workspace_root_ = canonical.string();
    workspace_access_ = access < 0 || access > 2 ? 1 : access;
}

void AgentEngine::reload_client() {
    client_.reset();
}

std::string AgentEngine::run(MessageList& messages, const AgentCallbacks& cb) {
    trace_.clear();
    remove_incomplete_tool_groups(messages);

    // messages is the caller's conversation log. On success we append the
    // assistant reply below; tool messages are appended inline during the loop.
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
            for (size_t i = 0; i < pending_tool_calls.size(); ++i) {
                auto& tc = pending_tool_calls[i];
                if (tc.name.empty() || !registry_.contains(tc.name)) {
                    const std::string label = tc.name.empty() ? "<empty>" : tc.name;
                    const std::string msg = "Model returned an unavailable tool: " + label;
                    if (cb.on_phase) cb.on_phase(AgentPhase::Error);
                    if (cb.on_error) cb.on_error(msg);
                    throw std::runtime_error(msg);
                }
                if (tc.id.empty()) {
                    tc.id = "call_" + std::to_string(round) + "_" +
                            std::to_string(i) + "_" + tc.name;
                }
                assistant_msg.tool_calls.push_back(std::move(tc));
            }
            // Keep the local message intact while executing its tool calls.
            // The conversation must contain this assistant message immediately
            // before the matching role=tool result messages.
            messages.push_back(assistant_msg);

            ToolContext ctx{workspace_root_, workspace_access_, nullptr};

            // Execution model (ported from harness tool-calls.ts):
            //  * concurrency-safe tools run on a bounded parallel pool;
            //  * exclusive tools run serially and act as a barrier;
            //  * results are collected in model order (exclusive tools also
            //    force everything to serialize for deterministic output).
            const bool any_exclusive = [&] {
                for (const auto& tc : assistant_msg.tool_calls)
                    if (!registry_.is_concurrency_safe(tc.name)) return true;
                return false;
            }();

            const size_t n = assistant_msg.tool_calls.size();
            std::vector<std::string> results(n);
            std::vector<bool> succeeded(n, true);
            std::vector<int64_t> durations(n, 0);

            auto execute_one = [&](size_t i) {
                const auto& tc = assistant_msg.tool_calls[i];
                auto t0 = std::chrono::steady_clock::now();
                std::string result_text;

                // Approval gate: dangerous tools must be confirmed first.
                if (registry_.requires_approval(tc.name) && cb.on_approval) {
                    if (!cb.on_approval(tc.name, tc.args)) {
                        auto t1 = std::chrono::steady_clock::now();
                        durations[i] = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
                        results[i] = "error: user denied approval for tool '" + tc.name + "'";
                        succeeded[i] = false;
                        return;
                    }
                }

                try {
                    json args = json::parse(tc.args.empty() ? "{}" : tc.args);
                    auto r = registry_.run(tc.name, args, ctx);
                    if (r) {
                        result_text = r->dump();
                    } else {
                        result_text = "error: unknown tool '" + tc.name + "'";
                        succeeded[i] = false;
                    }
                } catch (const std::exception& e) {
                    result_text = std::string("error: ") + e.what();
                    succeeded[i] = false;
                }
                auto t1 = std::chrono::steady_clock::now();
                durations[i] = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
                results[i] = result_text;
            };

            if (any_exclusive || n <= 1) {
                for (size_t i = 0; i < n; ++i) execute_one(i);   // serial
            } else {
                // Bounded parallel pool (max 4), results land in model order.
                const size_t pool = std::min<size_t>(n, 4);
                std::atomic<size_t> next{0};
                std::vector<std::thread> threads;
                threads.reserve(pool);
                for (size_t t = 0; t < pool; ++t) {
                    threads.emplace_back([&] {
                        while (true) {
                            size_t i = next.fetch_add(1);
                            if (i >= n) break;
                            execute_one(i);
                        }
                    });
                }
                for (auto& th : threads) th.join();
            }

            for (size_t i = 0; i < n; ++i) {
                const auto& tc = assistant_msg.tool_calls[i];
                ToolCallRecord rec;
                rec.id = tc.id;
                rec.name = tc.name;
                rec.args = tc.args.empty() ? "{}" : tc.args;
                rec.result = results[i];
                rec.succeeded = succeeded[i];
                rec.duration_us = durations[i];
                trace_.push_back(rec);
                if (cb.on_tool_call) cb.on_tool_call(rec);

                // Tool result message (role=tool) referencing the call id.
                messages.push_back({Role::Tool, results[i], {}, tc.id, results[i], false});
            }
            continue;   // loop: send updated conversation back to the model
        }

        // ── No tool calls → done: append the assistant reply to the log ──
        if (cb.on_phase) cb.on_phase(AgentPhase::Done);
        if (!assistant_text.empty()) {
            messages.push_back({Role::Assistant, assistant_text, {}, "", "", false});
        }
        return assistant_text;
    }

    // Tool-loop limit reached without a final answer.
    std::string msg = "Reached max tool rounds (" +
                      std::to_string(cfg_.max_tool_rounds) + ")";
    if (cb.on_phase) cb.on_phase(AgentPhase::Error);
    if (cb.on_error) cb.on_error(msg);
    throw std::runtime_error(msg);
}

std::string AgentEngine::summarize(const MessageList& prefix) {
    MessageList request;
    request.push_back({Role::System,
        "You are a conversation summariser. Compress the following chat "
        "history into a concise summary (2-5 sentences) that preserves: the "
        "user's goals, key facts the assistant learned, and the outcome of "
        "each tool call. Keep it in the same language as the conversation.",
        {}, "", "", false});
    for (const auto& m : prefix) request.push_back(m);
    request.push_back({Role::User, "Summarise the conversation above.",
                       {}, "", "", false});
    return client().complete(request);
}

} // namespace agent
