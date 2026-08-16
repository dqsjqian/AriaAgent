// AriaAgent — OpenAI-compatible LLM client (provider-agnostic).
//
// Implements the chat completions protocol shared by DeepSeek, OpenAI,
// Moonshot/Kimi, Qwen and others. The only DeepSeek-specific bit is the
// default base_url; everything else is the standard protocol.
#include "agent/llm_client.hpp"

#include <cstdlib>
#include <map>
#include <sstream>
#include <stdexcept>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace agent {

using json = nlohmann::json;

// ── ctor ────────────────────────────────────────────────────────────────────
OpenAiCompatClient::OpenAiCompatClient(Config cfg) : cfg_(std::move(cfg)) {
    if (cfg_.api_key.empty()) {
        if (const char* k = std::getenv("ARIA_LLM_API_KEY"); k && *k) {
            cfg_.api_key = k;
        }
    }
    if (cfg_.api_key.empty() && cfg_.base_url.find("deepseek") != std::string::npos) {
        if (const char* k = std::getenv("DEEPSEEK_API_KEY"); k && *k) {
            cfg_.api_key = k;
        }
    }
    if (cfg_.api_key.empty() && cfg_.base_url.find("openai") != std::string::npos) {
        if (const char* k = std::getenv("OPENAI_API_KEY"); k && *k) {
            cfg_.api_key = k;
        }
    }
    if (cfg_.api_key.empty()) {
        throw std::runtime_error(
            "No LLM API key configured. Set ARIA_LLM_API_KEY (or "
            "DEEPSEEK_API_KEY / OPENAI_API_KEY for the default endpoints).");
    }
    auth_header_ = "Bearer " + cfg_.api_key;
}

// ── URL parsing (host:port/path from base_url) ──────────────────────────────
namespace {
struct Endpoint {
    std::string host;
    int         port;
    std::string path;
};

Endpoint parse_base_url(const std::string& url, bool use_ssl) {
    Endpoint ep;
    std::string rest = url;
    const std::string scheme = use_ssl ? "https://" : "http://";
    if (rest.rfind(scheme, 0) == 0) {
        rest = rest.substr(scheme.size());
    }
    auto slash = rest.find('/');
    std::string hostport = rest.substr(0, slash);
    ep.path = slash == std::string::npos ? "/" : rest.substr(slash);
    auto colon = hostport.rfind(':');
    if (colon != std::string::npos) {
        ep.host = hostport.substr(0, colon);
        ep.port = std::atoi(hostport.substr(colon + 1).c_str());
    } else {
        ep.host = hostport;
        ep.port = use_ssl ? 443 : 80;
    }
    if (ep.path.empty()) ep.path = "/";
    return ep;
}
} // namespace

// ── Non-streaming completion ────────────────────────────────────────────────
std::string OpenAiCompatClient::complete(const MessageList& messages,
                                         const json& tools) {
    Endpoint ep = parse_base_url(cfg_.base_url, true);

    httplib::Client cli(ep.host, ep.port);
    cli.enable_server_certificate_verification(cfg_.verify_ssl);
    cli.set_connection_timeout(cfg_.timeout_sec, 0);
    cli.set_read_timeout(cfg_.timeout_sec, 0);

    json body;
    body["model"] = cfg_.model;
    body["stream"] = false;
    json arr = json::array();
    for (const auto& m : messages) arr.push_back(m.to_json());
    body["messages"] = std::move(arr);
    if (!tools.is_null() && !tools.empty()) body["tools"] = tools;

    auto res = cli.Post(ep.path + "/chat/completions",
                        {{"Authorization", auth_header_},
                         {"Content-Type", "application/json"}},
                        body.dump(), "application/json");
    if (!res) {
        throw std::runtime_error("LLM request failed: " +
                                 httplib::to_string(res.error()));
    }
    if (res->status != 200) {
        throw std::runtime_error("LLM HTTP " + std::to_string(res->status) +
                                 ": " + res->body.substr(0, 500));
    }
    json parsed = json::parse(res->body);
    return parsed["choices"][0]["message"]["content"].get<std::string>();
}

// ── Streaming completion (true token-by-token) ──────────────────────────────
void OpenAiCompatClient::complete_stream(
    const MessageList& messages,
    const json& tools,
    const std::function<void(const StreamEvent&)>& on_event) {
    Endpoint ep = parse_base_url(cfg_.base_url, true);

    httplib::Client cli(ep.host, ep.port);
    cli.enable_server_certificate_verification(cfg_.verify_ssl);
    cli.set_connection_timeout(cfg_.timeout_sec, 0);
    cli.set_read_timeout(cfg_.timeout_sec, 0);

    json body;
    body["model"] = cfg_.model;
    body["stream"] = true;
    json arr = json::array();
    for (const auto& m : messages) arr.push_back(m.to_json());
    body["messages"] = std::move(arr);
    if (!tools.is_null() && !tools.empty()) body["tools"] = tools;

    // Accumulate raw SSE chunks; parse "data: {...}" lines as they arrive.
    // Tool-call deltas: OpenAI streams each tool_call split across chunks,
    // identified by an integer index — merge fragments by that index.
    std::string sse_buffer;
    std::map<int, ToolCallInfo> tool_acc;
    auto emit_line = [&](const std::string& line) {
        if (line.empty()) return;
        const std::string prefix = "data:";
        if (line.rfind(prefix, 0) != 0) return;
        std::string payload = line.substr(prefix.size());
        if (!payload.empty() && payload[0] == ' ') payload.erase(0, 1);
        if (payload == "[DONE]") {
            StreamEvent e; e.finish = true; e.finish_reason = "stop";
            on_event(e);
            return;
        }
        try {
            json chunk = json::parse(payload);
            auto& choice = chunk["choices"][0];
            StreamEvent e;
            if (choice.contains("delta") && choice["delta"].contains("content")) {
                e.delta = choice["delta"]["content"].get<std::string>();
            }
            if (choice.contains("finish_reason") &&
                !choice["finish_reason"].is_null()) {
                e.finish = true;
                e.finish_reason = choice["finish_reason"].get<std::string>();
            }
            if (choice.contains("delta") && choice["delta"].contains("tool_calls")) {
                for (auto& tc : choice["delta"]["tool_calls"]) {
                    int idx = tc.value("index", 0);
                    auto& acc = tool_acc[idx];
                    if (tc.contains("id")) acc.id = tc["id"].get<std::string>();
                    auto& fn = tc["function"];
                    if (fn.contains("name")) acc.name = fn["name"].get<std::string>();
                    if (fn.contains("arguments")) acc.args += fn["arguments"].get<std::string>();
                }
                for (auto& [idx, tc] : tool_acc) e.tool_calls.push_back(tc);
            }
            on_event(e);
        } catch (const std::exception& ex) {
            (void)ex;   // skip malformed chunk, keep streaming
        }
    };

    // True streaming: httplib invokes this as chunks arrive from the socket.
    httplib::ContentReceiver receiver = [&](const char* data, size_t len) {
        sse_buffer.append(data, len);
        size_t pos = 0;
        while (true) {
            size_t nl = sse_buffer.find('\n', pos);
            if (nl == std::string::npos) break;
            std::string line = sse_buffer.substr(pos, nl - pos);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            emit_line(line);
            pos = nl + 1;
        }
        sse_buffer.erase(0, pos);
        return true;
    };

    auto res = cli.Post(ep.path + "/chat/completions",
                        {{"Authorization", auth_header_},
                         {"Content-Type", "application/json"}},
                        body.dump(), "application/json", receiver);
    if (!res) {
        throw std::runtime_error("LLM stream failed: " +
                                 httplib::to_string(res.error()));
    }
    if (res->status != 200) {
        throw std::runtime_error("LLM HTTP " + std::to_string(res->status) +
                                 ": " + res->body.substr(0, 500));
    }
}

// ── Factory ─────────────────────────────────────────────────────────────────
std::unique_ptr<LlmClient> create_llm_client(const LlmClient::Config& overrides) {
    LlmClient::Config cfg;
    cfg.base_url = overrides.base_url.empty()
        ? "https://api.deepseek.com" : overrides.base_url;
    cfg.model = overrides.model.empty() ? "deepseek-chat" : overrides.model;
    cfg.timeout_sec = overrides.timeout_sec;
    cfg.verify_ssl = overrides.verify_ssl;

    if (const char* v = std::getenv("ARIA_LLM_BASE_URL"); v && *v) cfg.base_url = v;
    if (const char* v = std::getenv("ARIA_LLM_MODEL"); v && *v) cfg.model = v;
    if (!overrides.api_key.empty()) cfg.api_key = overrides.api_key;

    auto client = std::make_unique<OpenAiCompatClient>(cfg);
    return client;
}

// ── Tool schema ─────────────────────────────────────────────────────────────
json build_tools_schema(const std::vector<Tool>& tools) {
    json arr = json::array();
    for (const auto& t : tools) {
        arr.push_back({
            {"type", "function"},
            {"function", {
                {"name", t.name},
                {"description", t.description},
                {"parameters", t.parameters}
            }}
        });
    }
    return arr;
}

} // namespace agent
