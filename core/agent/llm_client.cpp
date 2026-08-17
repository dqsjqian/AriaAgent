// AriaAgent — OpenAI-compatible LLM client (provider-agnostic).
//
// Implements the chat completions protocol shared by DeepSeek, OpenAI,
// Moonshot/Kimi, Qwen and others. The only DeepSeek-specific bit is the
// default base_url; everything else is the standard protocol.
#include "agent/llm_client.hpp"

#include <algorithm>
#include <cctype>
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
    std::string origin;
    std::string path;
};

Endpoint parse_endpoint(std::string url) {
    while (!url.empty() && std::isspace(static_cast<unsigned char>(url.front()))) {
        url.erase(url.begin());
    }
    while (!url.empty() && std::isspace(static_cast<unsigned char>(url.back()))) {
        url.pop_back();
    }
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
        throw std::invalid_argument("LLM URL must start with http:// or https://");
    }

    const auto authority_begin = url.find("://") + 3;
    const auto path_begin = url.find('/', authority_begin);
    Endpoint ep;
    ep.origin = path_begin == std::string::npos ? url : url.substr(0, path_begin);
    ep.path = path_begin == std::string::npos ? "" : url.substr(path_begin);
    while (ep.path.size() > 1 && ep.path.back() == '/') ep.path.pop_back();
    if (ep.origin.size() == authority_begin) {
        throw std::invalid_argument("LLM URL is missing a host");
    }

    constexpr const char* suffix = "/chat/completions";
    if (ep.path.size() < std::char_traits<char>::length(suffix) ||
        ep.path.compare(ep.path.size() - std::char_traits<char>::length(suffix),
                        std::char_traits<char>::length(suffix), suffix) != 0) {
        ep.path += suffix;
    }
    return ep;
}

std::string request_error(const char* operation, const Endpoint& ep,
                          const httplib::Result& result) {
    return std::string(operation) + " failed for " + ep.origin + ep.path + ": " +
           httplib::to_string(result.error());
}

std::string http_error_detail(const std::string& body) {
    if (body.empty()) return "server returned an empty error response";
    try {
        const auto parsed = json::parse(body);
        if (parsed.contains("error")) {
            const auto& error = parsed["error"];
            if (error.is_string()) return error.get<std::string>().substr(0, 500);
            if (error.is_object() && error.contains("message") &&
                error["message"].is_string()) {
                return error["message"].get<std::string>().substr(0, 500);
            }
        }
        if (parsed.contains("message") && parsed["message"].is_string()) {
            return parsed["message"].get<std::string>().substr(0, 500);
        }
        if (parsed.contains("detail") && parsed["detail"].is_string()) {
            return parsed["detail"].get<std::string>().substr(0, 500);
        }
    } catch (const json::exception&) {
        // Fall back to the bounded plain-text response below.
    }
    std::string detail = body.substr(0, 500);
    std::replace(detail.begin(), detail.end(), '\n', ' ');
    std::replace(detail.begin(), detail.end(), '\r', ' ');
    return detail;
}
} // namespace

// ── Non-streaming completion ────────────────────────────────────────────────
std::string OpenAiCompatClient::complete(const MessageList& messages,
                                         const json& tools) {
    const Endpoint ep = parse_endpoint(cfg_.base_url);

    httplib::Client cli(ep.origin);
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

    auto res = cli.Post(ep.path,
                        {{"Authorization", auth_header_}},
                        body.dump(), "application/json");
    if (!res) {
        throw std::runtime_error(request_error("LLM request", ep, res));
    }
    if (res->status != 200) {
        throw std::runtime_error("LLM HTTP " + std::to_string(res->status) +
                                 ": " + http_error_detail(res->body));
    }
    json parsed = json::parse(res->body);
    return parsed["choices"][0]["message"]["content"].get<std::string>();
}

// ── Streaming completion (true token-by-token) ──────────────────────────────
void OpenAiCompatClient::complete_stream(
    const MessageList& messages,
    const json& tools,
    const std::function<void(const StreamEvent&)>& on_event) {
    const Endpoint ep = parse_endpoint(cfg_.base_url);

    httplib::Client cli(ep.origin);
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
    std::string response_preview;
    std::map<int, ToolCallInfo> tool_acc;
    auto emit_line = [&](const std::string& line) {
        if (line.empty()) return;
        const std::string prefix = "data:";
        if (line.rfind(prefix, 0) != 0) return;
        std::string payload = line.substr(prefix.size());
        if (!payload.empty() && payload[0] == ' ') payload.erase(0, 1);
        if (payload == "[DONE]") {
            StreamEvent e; e.finish = true; e.finish_reason = "stop";
            for (const auto& [idx, tc] : tool_acc) {
                (void)idx;
                e.tool_calls.push_back(tc);
            }
            tool_acc.clear();
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
                    if (tc.contains("id") && tc["id"].is_string()) {
                        const auto id = tc["id"].get<std::string>();
                        if (!id.empty()) acc.id = id;
                    }
                    if (tc.contains("function")) {
                        auto& fn = tc["function"];
                        if (fn.contains("name") && fn["name"].is_string()) {
                            const auto name = fn["name"].get<std::string>();
                            if (!name.empty()) acc.name = name;
                        }
                        if (fn.contains("arguments")) {
                            const auto part = fn["arguments"].get<std::string>();
                            // Most providers send argument deltas, while some
                            // OpenAI-compatible gateways send the full argument
                            // snapshot on every chunk. Support both forms.
                            if (!acc.args.empty() && part.rfind(acc.args, 0) == 0) {
                                acc.args = part;
                            } else if (acc.args.rfind(part, 0) != 0) {
                                acc.args += part;
                            }
                        }
                    }
                }
            }
            if (e.finish && !tool_acc.empty()) {
                for (const auto& [idx, tc] : tool_acc) {
                    (void)idx;
                    e.tool_calls.push_back(tc);
                }
                tool_acc.clear();
            }
            on_event(e);
        } catch (const std::exception& ex) {
            (void)ex;   // skip malformed chunk, keep streaming
        }
    };

    // True streaming: httplib invokes this as chunks arrive from the socket.
    httplib::ContentReceiver receiver = [&](const char* data, size_t len) {
        if (response_preview.size() < 4096) {
            response_preview.append(data, std::min(len, 4096 - response_preview.size()));
        }
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

    auto res = cli.Post(ep.path,
                        {{"Authorization", auth_header_},
                         {"Accept", "text/event-stream"}},
                        body.dump(), "application/json", receiver);
    if (!res) {
        throw std::runtime_error(request_error("LLM stream", ep, res));
    }
    if (res->status != 200) {
        throw std::runtime_error("LLM HTTP " + std::to_string(res->status) +
                                 ": " + http_error_detail(response_preview));
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
