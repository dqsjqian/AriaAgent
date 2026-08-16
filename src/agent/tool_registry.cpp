// AriaAgent — tool registry implementation + built-in tools.
#include "agent/tool_registry.hpp"

#include <chrono>
#include <cmath>
#include <ctime>
#include <stdexcept>

namespace agent {

using json = nlohmann::json;

void ToolRegistry::register_tool(Tool tool) {
    index_[tool.name] = tools_.size();
    tools_.push_back(std::move(tool));
}

std::optional<json> ToolRegistry::run(const std::string& name,
                                      const json& args,
                                      ToolContext& ctx) const {
    auto it = index_.find(name);
    if (it == index_.end()) return std::nullopt;
    const Tool& t = tools_[it->second];
    if (!t.fn) return json{{"error", "tool has no implementation"}};
    return t.fn(args, ctx);
}

json ToolRegistry::schema() const {
    json arr = json::array();
    for (const auto& t : tools_) {
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

// ── Built-in tools ──────────────────────────────────────────────────────────
namespace {

json tool_calculator(const json& args, ToolContext&) {
    double a = args.value("a", 0.0);
    double b = args.value("b", 0.0);
    std::string op = args.value("op", "add");
    double r = 0;
    if (op == "add") r = a + b;
    else if (op == "sub") r = a - b;
    else if (op == "mul") r = a * b;
    else if (op == "div") { if (b == 0) return json{{"error", "division by zero"}}; r = a / b; }
    else if (op == "pow") r = std::pow(a, b);
    else return json{{"error", "unknown op: " + op}};
    return json{{"result", r}};
}

json tool_now(const json&, ToolContext&) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return json{{"now", buf}};
}

} // namespace

void register_builtin_tools(ToolRegistry& reg) {
    reg.register_tool({
        "calculator",
        "Evaluate a simple arithmetic expression with two operands.",
        {
            {"type", "object"},
            {"properties", {
                {"a", {{"type", "number"}}},
                {"b", {{"type", "number"}}},
                {"op", {{"type", "string"},
                        {"enum", json::array({"add","sub","mul","div","pow"})}}}
            }},
            {"required", json::array({"a","b","op"})}
        },
        tool_calculator
    });
    reg.register_tool({
        "current_time",
        "Return the current local date and time.",
        {{"type", "object"}, {"properties", json::object()}},
        tool_now
    });
}

} // namespace agent
