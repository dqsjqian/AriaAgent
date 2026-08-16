// AriaAgent — filesystem tools (read/write/list/edit).
//
// Ported from harness packages/fs: read_file / write_file / edit_file with
// workspace-path sandboxing (paths outside the workspace root are refused).
#pragma once

#include "agent/tool_registry.hpp"

namespace agent {

// Registers: read_file, write_file, edit_file.
void register_fs_tools(ToolRegistry& reg);

} // namespace agent
