// AriaAgent — LLM client abstraction (provider-agnostic).
//
// AriaAgent talks to any OpenAI-compatible chat API (DeepSeek, OpenAI,
// Moonshot/Kimi, Qwen, GLM, …) — they all speak the same HTTP protocol,
// differing only in base_url / model / api_key. This header defines the
// abstract `LlmClient` interface and the concrete `OpenAiCompatClient`
// implementation. Nothing else in the codebase knows (or cares) which
// provider is wired in.
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "agent/model.hpp"

namespace agent {

// Streamed completion events delivered to the caller as they arrive.
struct StreamEvent {
    std::string delta;                     // text delta (assistant content)
    std::vector<ToolCallInfo> tool_calls;  // tool_calls merged by index
    bool finish{false};
    std::string finish_reason;
};

// ── Abstract client ─────────────────────────────────────────────────────────
class LlmClient {
public:
    virtual ~LlmClient() = default;

    // Provider config common to every OpenAI-compatible endpoint.
    struct Config {
        std::string base_url;      // e.g. https://api.deepseek.com
        std::string api_key;       // from env if empty (see factory)
        std::string model;         // e.g. deepseek-chat / gpt-4o-mini / qwen-plus
        int         timeout_sec{120};
        bool        verify_ssl{true};
    };

    // Non-streaming completion: returns final assistant content.
    // tools: OpenAI-format tool schema array (may be empty → no tools).
    virtual std::string complete(const MessageList& messages,
                                 const nlohmann::json& tools = {}) = 0;

    // Streaming completion: invokes on_event for each SSE delta (token-level).
    virtual void complete_stream(
        const MessageList& messages,
        const nlohmann::json& tools,
        const std::function<void(const StreamEvent&)>& on_event) = 0;
};

// ── OpenAI-compatible implementation ────────────────────────────────────────
class OpenAiCompatClient : public LlmClient {
public:
    explicit OpenAiCompatClient(Config cfg);

    std::string complete(const MessageList& messages,
                         const nlohmann::json& tools = {}) override;
    void complete_stream(const MessageList& messages,
                         const nlohmann::json& tools,
                         const std::function<void(const StreamEvent&)>& on_event) override;

private:
    Config cfg_;
    std::string auth_header_;
};

// ── Factory ─────────────────────────────────────────────────────────────────
// Resolves configuration from environment variables so the binary is
// provider-neutral:
//   ARIA_LLM_BASE_URL   (default: https://api.deepseek.com)
//   ARIA_LLM_API_KEY    (fallback: DEEPSEEK_API_KEY / OPENAI_API_KEY)
//   ARIA_LLM_MODEL      (default: deepseek-chat)
// Returns nullptr if no usable api key is found.
std::unique_ptr<LlmClient> create_llm_client(const LlmClient::Config& overrides = {});

// Tool schema helpers — build the OpenAI "tools" array from Tool list.
nlohmann::json build_tools_schema(const std::vector<struct Tool>& tools);

} // namespace agent
