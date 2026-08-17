// AriaAgent — local Skill discovery and model-facing loader tool.
#pragma once

#include <string>

#include "agent/tool_registry.hpp"

namespace agent {

// Discover local SKILL.md bundles and register the read-only `use_skill` tool.
// The model receives only names/descriptions until it explicitly loads a skill.
void register_skill_tools(ToolRegistry& reg, const std::string& project_root);

} // namespace agent
