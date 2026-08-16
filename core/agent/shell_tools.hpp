// AriaAgent — shell / subprocess tools.
//
// Ported from harness packages/subprocess + packages/shell:
//   * run_command      — synchronous command, returns stdout/stderr/exit
//   * run_in_background — starts a command, returns a handle that can be
//     polled for incremental output (output spilled to a temp file)
//   * list_directory   — convenience for the agent to inspect the workspace
//
// Pure C++ (no Qt): process spawning goes through subprocess.hpp, directory
// listing through std::filesystem. The whole agent layer stays Qt-free so
// mobile shells (iOS/Android) can link it verbatim.
#pragma once

#include "agent/tool_registry.hpp"

namespace agent {

// Registers: run_command, run_in_background, read_output, kill_process,
// list_directory.
void register_shell_tools(ToolRegistry& reg);

} // namespace agent
