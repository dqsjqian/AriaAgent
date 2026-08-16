// AriaAgent — shared UI row types (framework-agnostic core).
//
// These are the row structs the View renders. They live in `core` so every
// platform shell (Qt / iOS / Android / Web) binds to the same shape.
#pragma once

#include <string>

#include "agent/model.hpp"

// ── UI message row (what the view renders) ──────────────────────────────────
struct UiMessage {
    agent::Role role;
    std::string author;      // stable identifier: "You" / "Agent" / "Tool"
                             // (UI logic keys on this; never translated)
    std::string display;     // localized header name (filled by the VM)
    std::string text;
    bool        is_tool{false};
    std::string tool_name;
};

// ── Tool call row (tool-chain timeline) ─────────────────────────────────────
struct UiToolCall {
    std::string name;
    std::string args;
    std::string result;
    bool        ok{true};
};
