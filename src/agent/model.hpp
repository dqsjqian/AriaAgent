// AriaAgent — core domain model shared by agent engine and UI layer.
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace agent {

// ── Chat message ────────────────────────────────────────────────────────────
enum class Role { User, Assistant, Tool, System };

inline const char* to_string(Role r) {
    switch (r) {
        case Role::User:     return "user";
        case Role::Assistant:return "assistant";
        case Role::Tool:     return "tool";
        case Role::System:   return "system";
    }
    return "user";
}

inline Role role_from_string(const std::string& s) {
    if (s == "assistant") return Role::Assistant;
    if (s == "tool")      return Role::Tool;
    if (s == "system")    return Role::System;
    return Role::User;
}

// A single function-call the assistant requested (OpenAI tool_calls format).
struct ToolCallInfo {
    std::string id;
    std::string name;
    std::string args;     // raw JSON arguments string
};

struct ChatMessage {
    Role            role{Role::User};
    std::string     content;          // text payload (tool result, assistant text)
    // Tool-call side channel (assistant messages).
    std::vector<ToolCallInfo> tool_calls;
    std::string     tool_call_id;     // tool message reference (role==Tool)
    std::string     tool_result;      // tool message content (role==Tool)
    bool            is_streaming{false};

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["role"] = to_string(role);
        if (role == Role::Tool) {
            j["content"] = tool_result.empty() ? content : tool_result;
            j["tool_call_id"] = tool_call_id;
        } else {
            j["content"] = content;
            if (role == Role::Assistant && !tool_calls.empty()) {
                j["tool_calls"] = nlohmann::json::array();
                for (const auto& tc : tool_calls) {
                    j["tool_calls"].push_back({
                        {"id", tc.id},
                        {"type", "function"},
                        {"function", {{"name", tc.name}, {"arguments", tc.args}}}
                    });
                }
            }
        }
        return j;
    }

    // Full serialization for session persistence (keeps every field).
    nlohmann::json to_storage_json() const {
        nlohmann::json j;
        j["role"] = to_string(role);
        j["content"] = content;
        j["tool_call_id"] = tool_call_id;
        j["tool_result"] = tool_result;
        j["is_streaming"] = is_streaming;
        j["tool_calls"] = nlohmann::json::array();
        for (const auto& tc : tool_calls) {
            j["tool_calls"].push_back({{"id", tc.id}, {"name", tc.name}, {"args", tc.args}});
        }
        return j;
    }

    static ChatMessage from_storage_json(const nlohmann::json& j) {
        ChatMessage m;
        m.role = role_from_string(j.value("role", "user"));
        m.content = j.value("content", "");
        m.tool_call_id = j.value("tool_call_id", "");
        m.tool_result = j.value("tool_result", "");
        m.is_streaming = j.value("is_streaming", false);
        if (j.contains("tool_calls")) {
            for (const auto& tc : j["tool_calls"]) {
                ToolCallInfo info;
                info.id = tc.value("id", "");
                info.name = tc.value("name", "");
                info.args = tc.value("args", "");
                m.tool_calls.push_back(std::move(info));
            }
        }
        return m;
    }
};

using MessageList = std::vector<ChatMessage>;

// ── Tool call record (UI trace) ─────────────────────────────────────────────
struct ToolCallRecord {
    std::string     id;
    std::string     name;
    std::string     args;       // pretty-printed args for display
    std::string     result;     // result text (may be truncated for display)
    bool            succeeded{true};
    int64_t         duration_us{0};
};

// ── Agent phases ────────────────────────────────────────────────────────────
enum class AgentPhase { Idle, Thinking, Tooling, Streaming, Done, Error };

inline const char* to_string(AgentPhase p) {
    switch (p) {
        case AgentPhase::Idle:      return "idle";
        case AgentPhase::Thinking:  return "thinking";
        case AgentPhase::Tooling:   return "tooling";
        case AgentPhase::Streaming: return "streaming";
        case AgentPhase::Done:      return "done";
        case AgentPhase::Error:     return "error";
    }
    return "idle";
}

// ── Agent reply (final result of one run) ───────────────────────────────────
struct AgentReply {
    std::string     text;               // final assistant text
    std::vector<ToolCallRecord> tools;  // every tool call made during run
    bool            ok{true};
    std::string     error;
};

// ── Tool interface ──────────────────────────────────────────────────────────
struct ToolContext {
    // Implementations may read system state / env here in the future.
};

struct Tool {
    std::string                 name;
    std::string                 description;
    nlohmann::json              parameters;    // JSON schema of args
    // Executes with parsed JSON args; returns result JSON.
    nlohmann::json              (*fn)(const nlohmann::json& args, ToolContext& ctx) = nullptr;
};

} // namespace agent
