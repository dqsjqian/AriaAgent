// AriaAgent — OpenAI-compatible LLM client (provider-agnostic).
//
// Implements the chat completions protocol shared by DeepSeek, OpenAI,
// Moonshot/Kimi, Qwen and others. The only DeepSeek-specific bit is the
// default base_url; everything else is the standard protocol.
//
// Transport: Continuo (github.com/dqsjqian/continuo) — the same
// coroutine-native C++23 networking library that powers Aria's HTTP
// adapter. Calls stay synchronous (they were with cpp-httplib too);
// internally each exchange drives an EventLoop to completion, resolves
// the host through Continuo's bounded system resolver, and streams the
// response body chunk by chunk. TLS always verifies the server
// certificate chain and hostname: Continuo ships no insecure bypass.
#include "agent/llm_client.hpp"

#include <continuo/core/event_loop.hpp>
#include <continuo/core/task.hpp>
#include <continuo/http/client.hpp>
#include <continuo/transport/resolver.hpp>
#include <continuo/transport/tcp.hpp>
#include <continuo/tls/context.hpp>
#include <continuo/tls/stream.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <map>
#include <span>
#include <stdexcept>
#include <utility>

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

// ── URL parsing (scheme, host, port, path from base_url) ────────────────────
namespace {

struct Endpoint {
    bool is_https = true;
    std::string host;
    std::uint16_t port = 443;
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

    Endpoint ep;
    ep.is_https = url.rfind("https://", 0) == 0;
    ep.port = ep.is_https ? 443 : 80;

    const auto authority_begin = url.find("://") + 3;
    const auto path_begin = url.find('/', authority_begin);
    std::string authority = path_begin == std::string::npos
        ? url.substr(authority_begin)
        : url.substr(authority_begin, path_begin - authority_begin);
    ep.path = path_begin == std::string::npos ? "" : url.substr(path_begin);
    while (ep.path.size() > 1 && ep.path.back() == '/') ep.path.pop_back();
    if (authority.empty()) {
        throw std::invalid_argument("LLM URL is missing a host");
    }

    // Optional :port on the authority.
    if (const auto colon = authority.rfind(':'); colon != std::string::npos) {
        const std::string_view port_text(authority.data() + colon + 1,
                                         authority.size() - colon - 1);
        int parsed = 0;
        // Raw pointers, not string_view iterators: MSVC's iterators are class
        // types and do not bind to from_chars' const char* overloads.
        const char* first = port_text.data();
        const char* last = port_text.data() + port_text.size();
        const auto [end, ec] = std::from_chars(first, last, parsed);
        if (ec == std::errc{} && end == last && parsed > 0 && parsed <= 65535) {
            ep.port = static_cast<std::uint16_t>(parsed);
            authority.resize(colon);
        }
    }
    ep.host = std::move(authority);

    constexpr const char* suffix = "/chat/completions";
    if (ep.path.size() < std::char_traits<char>::length(suffix) ||
        ep.path.compare(ep.path.size() - std::char_traits<char>::length(suffix),
                        std::char_traits<char>::length(suffix), suffix) != 0) {
        ep.path += suffix;
    }
    return ep;
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

// ── Continuo exchange ───────────────────────────────────────────────────────
//
// One blocking call drives one complete HTTP exchange to completion on a
// private EventLoop: bounded system resolution, TCP connect, an optional
// TLS handshake (always verified), the POST, and a chunk-by-chunk body read
// that feeds `on_chunk` as bytes arrive (SSE stays truly streaming).

struct ExchangeOutcome {
    long status = 0;
    std::string body;      // fully accumulated body (non-streaming / error paths)
    std::string error;     // transport failure description, empty on success
};

using ChunkSink = std::function<void(std::string_view)>;

continuo::Task<ExchangeOutcome>
exchange_task(continuo::EventLoop& loop,
              continuo::transport::Resolver& resolver,
              const Endpoint& ep,
              const std::string& host_header,
              const std::string& request_body,
              const std::string& auth_header,
              int timeout_sec,
              const ChunkSink& on_chunk) {
    using namespace continuo;
    ExchangeOutcome out;

    const auto timeout = std::chrono::seconds(timeout_sec > 0 ? timeout_sec : 120);
    const OperationOptions io{.stop = {}, .deadline = continuo::EventLoop::Clock::now() + timeout};

    auto resolved = co_await resolver.resolve(
        loop, continuo::transport::ResolveQuery{ep.host, std::to_string(ep.port)}, io);
    if (!resolved || resolved->empty()) {
        out.error = "DNS resolution failed for " + ep.host;
        co_return out;
    }

    // Try resolved endpoints in order; the first successful connection wins.
    std::optional<transport::tcp::Socket> socket;
    Error connect_error{};
    for (const auto& candidate : *resolved) {
        auto connected = co_await transport::tcp::connect(
            loop, candidate, {}, OperationOptions{.stop = {}, .deadline = io.deadline});
        if (connected) {
            socket = std::move(*connected);
            break;
        }
        connect_error = connected.error();
    }
    if (!socket) {
        out.error = std::string("connect failed for ") + ep.host + ":" +
                    std::to_string(ep.port) + ": " + connect_error.message();
        co_return out;
    }

    continuo::http::Request request;
    request.method = continuo::http::Method::post;
    request.target = ep.path;
    request.version = continuo::http::Version::http_1_1;
    request.headers.append("Host", host_header);
    request.headers.append("Authorization", auth_header);
    request.headers.append("Content-Type", "application/json");
    request.headers.append("Accept", "text/event-stream");
    request.headers.append("Connection", "close");

    const std::span<const std::byte> body_bytes{
        reinterpret_cast<const std::byte*>(request_body.data()), request_body.size()};

    // The two stream flavors differ only in the stream type threaded through
    // ClientConnection; the exchange logic is identical.
    if (ep.is_https) {
        auto context = continuo::tls::Context::client();
        if (!context) {
            out.error = "TLS context creation failed: " + context.error().message();
            co_return out;
        }
        auto tls_stream = continuo::tls::Stream<continuo::transport::tcp::Socket>::create(
            *socket, *context, ep.host);
        if (!tls_stream) {
            out.error = "TLS stream creation failed: " + tls_stream.error().message();
            co_return out;
        }
        auto handshake = co_await tls_stream->handshake(io);
        if (!handshake) {
            out.error = "TLS handshake failed: " + handshake.error().message();
            co_return out;
        }

        continuo::http::ClientConnection<continuo::tls::Stream<continuo::transport::tcp::Socket>>
            client(*tls_stream, {.request_timeout = timeout});
        auto started = co_await client.start(request, body_bytes, io);
        if (!started) {
            out.error = "LLM request failed: " + started.error().message();
            co_return out;
        }
        out.status = static_cast<long>(client.response().status);
        for (;;) {
            auto chunk = co_await client.read_body();
            if (!chunk) {
                out.error = "LLM response read failed: " + chunk.error().message();
                co_return out;
            }
            if (chunk->empty()) break;
            if (on_chunk) {
                on_chunk(std::string_view{reinterpret_cast<const char*>(chunk->data()),
                                          chunk->size()});
            }
            out.body.append(reinterpret_cast<const char*>(chunk->data()), chunk->size());
        }
        (void)co_await tls_stream->shutdown(io);
    } else {
        continuo::http::ClientConnection<continuo::transport::tcp::Socket>
            client(*socket, {.request_timeout = timeout});
        auto started = co_await client.start(request, body_bytes, io);
        if (!started) {
            out.error = "LLM request failed: " + started.error().message();
            co_return out;
        }
        out.status = static_cast<long>(client.response().status);
        for (;;) {
            auto chunk = co_await client.read_body();
            if (!chunk) {
                out.error = "LLM response read failed: " + chunk.error().message();
                co_return out;
            }
            if (chunk->empty()) break;
            if (on_chunk) {
                on_chunk(std::string_view{reinterpret_cast<const char*>(chunk->data()),
                                          chunk->size()});
            }
            out.body.append(reinterpret_cast<const char*>(chunk->data()), chunk->size());
        }
    }
    co_return out;
}

ExchangeOutcome run_exchange(const Endpoint& ep,
                             const std::string& host_header,
                             const std::string& request_body,
                             const std::string& auth_header,
                             int timeout_sec,
                             const ChunkSink& on_chunk) {
    using namespace continuo;
    auto loop = EventLoop::create();
    if (!loop) throw std::runtime_error("EventLoop creation failed");
    auto resolver = transport::Resolver::create();
    if (!resolver) throw std::runtime_error("Resolver creation failed");

    ExchangeOutcome outcome;
    // Named lambda on purpose: a coroutine called on a temporary closure
    // would leave the frame's `this` dangling after the full expression.
    auto exchange_root = [&]() -> Task<void> {
        outcome = co_await exchange_task(*loop, *resolver, ep, host_header,
                                         request_body, auth_header, timeout_sec,
                                         on_chunk);
    };
    Task<void> root = exchange_root();
    // Blocks until the exchange finishes and rethrows task exceptions — the
    // same blocking-call semantics the cpp-httplib implementation had.
    (void)loop->run_until_complete(std::move(root));
    return outcome;
}

}  // namespace

// ── Non-streaming completion ────────────────────────────────────────────────
std::string OpenAiCompatClient::complete(const MessageList& messages,
                                         const json& tools) {
    const Endpoint ep = parse_endpoint(cfg_.base_url);
    if (!cfg_.verify_ssl) {
        std::cerr << "[llm] verify_ssl=false is ignored: the Continuo transport "
                     "always verifies TLS certificates\n";
    }

    json body;
    body["model"] = cfg_.model;
    body["stream"] = false;
    json arr = json::array();
    for (const auto& m : messages) arr.push_back(m.to_json());
    body["messages"] = std::move(arr);
    if (!tools.is_null() && !tools.empty()) body["tools"] = tools;

    const std::string host_header =
        ep.port == (ep.is_https ? 443 : 80) ? ep.host
                                            : ep.host + ":" + std::to_string(ep.port);
    auto outcome = run_exchange(ep, host_header, body.dump(), auth_header_,
                                cfg_.timeout_sec, {});
    if (!outcome.error.empty()) {
        throw std::runtime_error("LLM request failed for " + ep.host + ep.path +
                                 ": " + outcome.error);
    }
    if (outcome.status != 200) {
        throw std::runtime_error("LLM HTTP " + std::to_string(outcome.status) +
                                 ": " + http_error_detail(outcome.body));
    }
    json parsed = json::parse(outcome.body);
    return parsed["choices"][0]["message"]["content"].get<std::string>();
}

// ── Streaming completion (true token-by-token) ──────────────────────────────
void OpenAiCompatClient::complete_stream(
    const MessageList& messages,
    const json& tools,
    const std::function<void(const StreamEvent&)>& on_event) {
    const Endpoint ep = parse_endpoint(cfg_.base_url);
    if (!cfg_.verify_ssl) {
        std::cerr << "[llm] verify_ssl=false is ignored: the Continuo transport "
                     "always verifies TLS certificates\n";
    }

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

    // True streaming: Continuo hands raw body chunks to this sink as they
    // arrive from the socket.
    ChunkSink receiver = [&](std::string_view data) {
        const auto take = std::min<std::size_t>(data.size(),
                                                4096 - std::min<std::size_t>(response_preview.size(), 4096));
        if (take > 0) response_preview.append(data.substr(0, take));
        sse_buffer.append(data);
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
    };

    const std::string host_header =
        ep.port == (ep.is_https ? 443 : 80) ? ep.host
                                            : ep.host + ":" + std::to_string(ep.port);
    auto outcome = run_exchange(ep, host_header, body.dump(), auth_header_,
                                cfg_.timeout_sec, receiver);
    if (!outcome.error.empty()) {
        throw std::runtime_error("LLM stream failed for " + ep.host + ep.path +
                                 ": " + outcome.error);
    }
    if (outcome.status != 200) {
        throw std::runtime_error("LLM HTTP " + std::to_string(outcome.status) +
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

}  // namespace agent
