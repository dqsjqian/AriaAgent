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

    // Whether a tool with this exact name is registered.
    bool contains(const std::string& name) const;

    // Whether a tool may run concurrently with others (unknown → false).
    bool is_concurrency_safe(const std::string& name) const;

    // Whether a tool needs user approval before execution (unknown → false).
    bool requires_approval(const std::string& name) const;

    // Add model-facing runtime guidance associated with registered tools.
    void add_prompt_guidance(std::string guidance);
    const std::vector<std::string>& prompt_guidance() const { return prompt_guidance_; }

private:
    std::vector<Tool> tools_;
    std::map<std::string, size_t> index_;
    std::vector<std::string> prompt_guidance_;
};

// Built-in tools.
void register_builtin_tools(ToolRegistry& reg);

} // namespace agent
