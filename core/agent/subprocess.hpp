// AriaAgent — cross-platform subprocess utilities (no Qt, no external deps).
//
// Minimal shell-out support for the agent tools:
//   - run_sync():        run a command, capture merged output, enforce a timeout
//   - background spawn / incremental read / kill by opaque handle
//
// Implementations: Win32 CreateProcess + pipes; POSIX fork/exec + pipes.
#pragma once

#include <string>

namespace agent {

/// Result of a synchronous command run.
struct ProcResult {
    int         exit_code{0};
    bool        timed_out{false};
    std::string output;          // merged stdout+stderr
};

/// Run `command` via the platform shell, blocking up to `timeout_ms`
/// (0 = no timeout). Kills the process tree on timeout.
ProcResult run_sync(const std::string& command, int timeout_ms,
                    const std::string& working_directory = {});

/// Opaque handle for a background process.
struct BgProc;

/// Start `command` detached. Returns a handle or nullptr on failure.
/// The returned pointer must be released with bg_close() (or kept for the
/// process lifetime and cleaned up at app exit).
BgProc* bg_start(const std::string& command,
                 const std::string& working_directory = {});

/// Consume output accumulated since the last call.
std::string bg_read(BgProc* p);

/// True if the process is still running.
bool bg_running(BgProc* p);

/// Exit code (valid only when not running).
int bg_exit_code(BgProc* p);

/// Terminate the process (SIGKILL / TerminateProcess).
bool bg_kill(BgProc* p);

/// Close the handle, stopping any reader thread. Safe to call once.
void bg_close(BgProc* p);

} // namespace agent
