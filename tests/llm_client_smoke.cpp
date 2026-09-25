// AriaAgent — functional smoke test for the Continuo-backed LLM client.
//
// Spins up a real Continuo HTTP server on 127.0.0.1 (port 0), points
// OpenAiCompatClient at it, and drives one non-streaming completion and one
// streaming completion through the full stack: URL parsing, request
// encoding, response reading, and SSE parsing. No network, no TLS (the
// always-verifying TLS path is covered by Continuo's own test suite).
#include "agent/llm_client.hpp"
#include "agent/model.hpp"

#include <continuo/core/event_loop.hpp>
#include <continuo/core/task.hpp>
#include <continuo/core/task_scope.hpp>
#include <continuo/http/connection.hpp>
#include <continuo/http/message.hpp>
#include <continuo/transport/tcp.hpp>

#include <cstdio>
#include <cstring>
#include <future>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using continuo::OperationOptions;
using continuo::Result;
using continuo::Task;
using continuo::transport::tcp::Listener;
using continuo::transport::tcp::Socket;

std::string_view body_text(std::span<const std::byte> body) {
    return {reinterpret_cast<const char*>(body.data()), body.size()};
}

// One mock server thread: accept loop + per-connection request handling on
// a private EventLoop. Shuts down when the caller requests stop.
struct MockServer {
    std::thread thread;
    std::promise<std::uint16_t> port_promise;
    std::stop_source stop;

    void start() {
        thread = std::thread([this] {
            auto loop = continuo::EventLoop::create();
            if (!loop) { port_promise.set_value(0); return; }
            auto listener = Listener::bind(*loop,
                continuo::transport::Endpoint::loopback(0));
            if (!listener) { port_promise.set_value(0); return; }
            port_promise.set_value(listener->local_endpoint().port());

            continuo::TaskScope scope;
            // Named lambda on purpose: a coroutine called on a temporary
            // closure would leave the frame's `this` dangling after the
            // full expression ends.
            auto accept_root = [&]() -> Task<void> {
                for (;;) {
                    auto peer = co_await listener->accept(
                        OperationOptions{.stop = stop.get_token()});
                    if (!peer) break;
                    scope.spawn(handle(std::move(*peer)));
                }
                co_await scope.join();
            };
            auto root = accept_root();
            (void)loop->run_until_complete(std::move(root));
        });
    }

    static Task<void> handle(Socket socket) {
        auto handler = [](const continuo::http::Request& request,
                          continuo::http::ResponseWriter<Socket>& writer,
                          std::span<const std::byte> body) -> Task<Result<void>> {
            const bool wants_stream =
                body_text(body).find("\"stream\":true") != std::string_view::npos;
            (void)request;
            continuo::http::Response response;
            response.status = 200;
            if (!wants_stream) {
                const std::string payload =
                    R"({"choices":[{"message":{"content":"hello from continuo"}}]})";
                response.headers.append("Content-Type", "application/json");
                co_return co_await writer.send(response,
                    std::span<const std::byte>(
                        reinterpret_cast<const std::byte*>(payload.data()),
                        payload.size()));
            }
            response.headers.append("Content-Type", "text/event-stream");
            co_await writer.send_head_chunked(response);
            const char* frames[] = {
                "data: {\"choices\":[{\"delta\":{\"content\":\"hel\"}}]}\n\n",
                "data: {\"choices\":[{\"delta\":{\"content\":\"lo\"}}]}\n\n",
                "data: [DONE]\n\n",
            };
            for (const char* frame : frames) {
                co_await writer.write(std::span<const std::byte>(
                    reinterpret_cast<const std::byte*>(frame),
                    std::strlen(frame)));
            }
            co_return co_await writer.finish();
        };
        co_await continuo::http::serve_connection(socket, handler);
    }

    [[nodiscard]] std::uint16_t wait_port() {
        return port_promise.get_future().get();
    }

    void shutdown() {
        stop.request_stop();
        if (thread.joinable()) thread.join();
    }
};

int failures = 0;

void check(bool ok, const char* what) {
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

}  // namespace

int main() {
    MockServer server;
    server.start();
    const std::uint16_t port = server.wait_port();
    if (port == 0) {
        std::printf("[FAIL] mock server failed to start\n");
        return 1;
    }

    agent::LlmClient::Config cfg;
    cfg.base_url = "http://127.0.0.1:" + std::to_string(port) + "/v1";
    cfg.api_key = "test-key";
    cfg.model = "mock-model";
    agent::OpenAiCompatClient client{cfg};

    agent::MessageList messages;
    messages.push_back(agent::ChatMessage{agent::Role::User, "hi", {}});

    // 1. Non-streaming completion.
    try {
        const std::string content = client.complete(messages);
        check(content == "hello from continuo",
              "complete() returns the mock assistant content");
    } catch (const std::exception& e) {
        std::printf("[FAIL] complete() threw: %s\n", e.what());
        ++failures;
    }

    // 2. Streaming completion: deltas arrive, in order, with a finish event.
    try {
        std::string streamed;
        bool finished = false;
        std::string finish_reason;
        client.complete_stream(messages, {}, [&](const agent::StreamEvent& e) {
            streamed += e.delta;
            if (e.finish) {
                finished = true;
                finish_reason = e.finish_reason;
            }
        });
        check(streamed == "hello", "complete_stream() reassembles deltas");
        check(finished, "complete_stream() reports the finish event");
        check(finish_reason == "stop", "finish_reason is stop");
    } catch (const std::exception& e) {
        std::printf("[FAIL] complete_stream() threw: %s\n", e.what());
        ++failures;
    }

    server.shutdown();
    std::printf(failures == 0 ? "SMOKE OK\n" : "SMOKE FAILED\n");
    return failures == 0 ? 0 : 1;
}
