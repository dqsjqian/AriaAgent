// AriaAgent — tool registry: register, discover and execute tools.
#pragma once

#include <map>
#include <vector>
#include <string>
#include <optional>

#include <nlohmann/json.hpp>

#include "agent/json_schema.hpp"
#include "agent/model.hpp"

namespace agent {

class ToolRegistry {
public:
    void register_tool(Tool tool);
    const std::vector<Tool>& tools() const { return tools_; }

    // Returns nullopt if the tool is unknown.
    std::optional<nlohmann::json> run(const std::string& name,
                                      const nlohmann::json& args,
                                      ToolContext& ctx) const;

    // Schema array for the OpenAI "tools" request field.
    nlohmann::json schema() const;

    // Whether a tool may run concurrently with others (unknown → false).
    bool is_concurrency_safe(const std::string& name) const;

private:
    std::vector<Tool> tools_;
    std::map<std::string, size_t> index_;
};

// Built-in tools.
void register_builtin_tools(ToolRegistry& reg);

} // namespace agent
