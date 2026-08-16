// AriaAgent — todo tool: agent-visible task list (ported from harness todo).
#pragma once

#include "agent/todo_store.hpp"
#include "agent/tool_registry.hpp"

namespace agent {

// Registers: todo_set (replace all), todo_add, todo_list.
// Operates on TodoStore::instance().
void register_todo_tools(ToolRegistry& reg);

} // namespace agent
