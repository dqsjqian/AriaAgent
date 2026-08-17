// AriaAgent — canonical workspace path resolution shared by local tools.
#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "agent/model.hpp"

namespace agent {

// Resolve `input` against ctx.workspace_root and reject traversal/symlink escape.
// Set require_existing=false for a file that is about to be created.
std::optional<std::filesystem::path> resolve_workspace_path(
    const ToolContext& ctx, const std::string& input, bool require_existing,
    std::string* error);

} // namespace agent
