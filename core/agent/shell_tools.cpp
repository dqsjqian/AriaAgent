// AriaAgent — shell / subprocess tools implementation (pure C++, no Qt).
#include "agent/shell_tools.hpp"

#include "agent/subprocess.hpp"
#include "agent/workspace.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>

namespace agent {

using json = nlohmann::json;

namespace {

// A working directory is not an OS sandbox. Only Full Access may execute
// arbitrary shell commands; lower modes use the path-aware file tools.
std::optional<json> shell_access_deny(const ToolContext& ctx, const char* op) {
    if (ctx.workspace_access != 2) {
        return json{{"error", op}, {"reason",
            "shell execution requires Full Access; use workspace file tools "
            "in Read Only or Workspace Write mode"}};
    }
    return std::nullopt;
}

// ── Background process registry (handle → BgProc*) ─────────────────────────
struct ProcRegistry {
    std::mutex mu;
    std::map<int, BgProc*> procs;
    int next_handle{1};
};
ProcRegistry& procs() {
    static ProcRegistry r;
    return r;
}

json run_command_impl(const json& args, ToolContext& ctx) {
    if (auto deny = shell_access_deny(ctx, "run_command")) return *deny;
    const std::string cmd = args.value("command", "");
    const int timeout_ms = args.value("timeout_ms", 30000);
    if (cmd.empty()) return json{{"error", "command is required"}};

    const auto t0 = std::chrono::steady_clock::now();
    const ProcResult r = run_sync(cmd, timeout_ms, ctx.workspace_root);
    const auto t1 = std::chrono::steady_clock::now();
    return json{
        {"exit_code", r.exit_code},
        {"timed_out", r.timed_out},
        {"output", r.output},
        {"duration_ms", std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()},
    };
}

json run_background_impl(const json& args, ToolContext& ctx) {
    if (auto deny = shell_access_deny(ctx, "run_in_background")) return *deny;
    const std::string cmd = args.value("command", "");
    if (cmd.empty()) return json{{"error", "command is required"}};

    BgProc* p = bg_start(cmd, ctx.workspace_root);
    if (!p) return json{{"error", "failed to start command"}};
    std::lock_guard<std::mutex> lk(procs().mu);
    const int handle = procs().next_handle++;
    procs().procs[handle] = p;
    return json{{"handle", handle}, {"started", true}};
}

json read_output_impl(const json& args, ToolContext&) {
    const int handle = args.value("handle", -1);
    std::lock_guard<std::mutex> lk(procs().mu);
    auto it = procs().procs.find(handle);
    if (it == procs().procs.end())
        return json{{"error", "unknown process handle"}};
    BgProc* p = it->second;
    return json{
        {"output", bg_read(p)},
        {"running", bg_running(p)},
        {"exit_code", bg_running(p) ? 0 : bg_exit_code(p)},
    };
}

json kill_process_impl(const json& args, ToolContext&) {
    const int handle = args.value("handle", -1);
    std::lock_guard<std::mutex> lk(procs().mu);
    auto it = procs().procs.find(handle);
    if (it == procs().procs.end())
        return json{{"error", "unknown process handle"}};
    BgProc* p = it->second;
    bg_kill(p);
    bg_close(p);
    procs().procs.erase(it);
    return json{{"killed", true}};
}

json list_directory_impl(const json& args, ToolContext& ctx) {
    const std::string path = args.value("path", ".");
    std::string error;
    const auto resolved = resolve_workspace_path(ctx, path, true, &error);
    if (!resolved) return json{{"error", error}};
    std::error_code ec;
    std::filesystem::directory_iterator it(*resolved, ec);
    if (ec) return json{{"error", "directory does not exist: " + path}};

    json items = json::array();
    std::vector<std::filesystem::directory_entry> entries;
    for (const auto& e : it) entries.push_back(e);
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) {
                  bool ad = a.is_directory(), bd = b.is_directory();
                  if (ad != bd) return ad;             // dirs first
                  return a.path().filename().string() < b.path().filename().string();
              });
    for (const auto& e : entries) {
        bool is_dir = e.is_directory();
        items.push_back({
            {"name", e.path().filename().string()},
            {"type", is_dir ? "dir" : "file"},
            {"size", is_dir ? 0 : e.file_size()},
        });
    }
    return json{{"items", items}, {"count", items.size()}};
}

} // namespace

void stop_all_background_processes() {
    std::lock_guard<std::mutex> lk(procs().mu);
    for (auto& [handle, process] : procs().procs) {
        (void)handle;
        bg_kill(process);
        bg_close(process);
    }
    procs().procs.clear();
}

void register_shell_tools(ToolRegistry& reg) {
    reg.register_tool({
        "run_command",
        "Run a shell command synchronously and return its output. "
        "Use for quick operations like git status, ls, or compiling.",
        {
            {"type", "object"},
            {"properties", {
                {"command", {{"type", "string"}}},
                {"timeout_ms", {{"type", "integer"}, {"minimum", 100}, {"maximum", 120000}}}
            }},
            {"required", json::array({"command"})}
        },
        false,               // exclusive: shell runs serialize
        true,                // requires approval (executes commands)
        run_command_impl
    });
    reg.register_tool({
        "run_in_background",
        "Start a command in the background and get a handle. "
        "Use for long-running processes (servers, builds).",
        {
            {"type", "object"},
            {"properties", {
                {"command", {{"type", "string"}}}
            }},
            {"required", json::array({"command"})}
        },
        false,
        true,                // requires approval (spawns processes)
        run_background_impl
    });
    reg.register_tool({
        "read_output",
        "Read incremental output from a background process by handle.",
        {
            {"type", "object"},
            {"properties", {
                {"handle", {{"type", "integer"}, {"minimum", 1}}}
            }},
            {"required", json::array({"handle"})}
        },
        true,                // concurrent reads are fine
        false,
        read_output_impl
    });
    reg.register_tool({
        "kill_process",
        "Terminate a background process by handle.",
        {
            {"type", "object"},
            {"properties", {
                {"handle", {{"type", "integer"}, {"minimum", 1}}}
            }},
            {"required", json::array({"handle"})}
        },
        false,
        true,                // requires approval (kills processes)
        kill_process_impl
    });
    reg.register_tool({
        "list_directory",
        "List files and directories in a path (default: workspace root).",
        {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}}}
            }},
            {"required", json::array()}
        },
        true,
        false,
        list_directory_impl
    });
}

} // namespace agent