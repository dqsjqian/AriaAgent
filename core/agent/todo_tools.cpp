// AriaAgent — todo tool implementation.
#include "agent/todo_tools.hpp"

namespace agent {

using json = nlohmann::json;

namespace {

// ── JSON encoding / decoding helpers ────────────────────────────────────────
json item_to_json(const TodoItem& it) {
    return {{"content", it.content}, {"status", todo_status_str(it.status)}};
}

TodoItem item_from_json(const json& j) {
    TodoItem it;
    it.content = j.value("content", "");
    const std::string s = j.value("status", "pending");
    if (s == "done") it.status = TodoStatus::Done;
    else if (s == "in_progress") it.status = TodoStatus::InProgress;
    return it;
}

// Shared mutable store (singleton).
TodoStore& store() { return TodoStore::instance(); }

json todo_set_impl(const json& args, ToolContext&) {
    std::vector<TodoItem> items;
    if (args.contains("items") && args["items"].is_array()) {
        for (const auto& e : args["items"]) items.push_back(item_from_json(e));
    }
    store().replace(std::move(items));
    return json{{"ok", true}, {"count", items.size()}};
}

json todo_add_impl(const json& args, ToolContext&) {
    auto items = store().snapshot();
    items.push_back({args.value("content", ""), TodoStatus::Pending});
    store().replace(std::move(items));
    return json{{"ok", true}};
}

json todo_list_impl(const json&, ToolContext&) {
    json arr = json::array();
    for (const auto& it : store().snapshot()) arr.push_back(item_to_json(it));
    return json{{"todos", arr}, {"count", arr.size()}};
}

} // namespace

void register_todo_tools(ToolRegistry& reg) {
    reg.register_tool({
        "todo_set",
        "Replace the entire todo list with the given items. "
        "Each item: {content: string, status: pending|in_progress|done}.",
        {
            {"type", "object"},
            {"properties", {
                {"items", {{"type", "array"},
                           {"items", {{"type", "object"}}}}}
            }},
            {"required", json::array({"items"})}
        },
        false,   // exclusive: snapshot mutations serialize
        false,   // requires approval
        todo_set_impl
    });
    reg.register_tool({
        "todo_add",
        "Append a single todo item.",
        {
            {"type", "object"},
            {"properties", {
                {"content", {{"type", "string"}}}
            }},
            {"required", json::array({"content"})}
        },
        false,
        false,
        todo_add_impl
    });
    reg.register_tool({
        "todo_list",
        "Return the current todo list.",
        {{"type", "object"}, {"properties", json::object()}},
        true,   // concurrent reads are fine
        false,
        todo_list_impl
    });
}

} // namespace agent
