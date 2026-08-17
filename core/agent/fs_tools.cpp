// AriaAgent — filesystem tools implementation (pure C++, no Qt).
#include "agent/fs_tools.hpp"

#include "agent/workspace.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace agent {

using json = nlohmann::json;

namespace {

// Read-only mode guard (mirrors shell_tools::ws_read_only_deny).
std::optional<json> ws_read_only_deny(const ToolContext& ctx, const char* op) {
    if (ctx.workspace_access == 0) {
        return json{{"error", op}, {"reason",
            "workspace mode is Read Only — switch the input bar dropdown "
            "to Workspace Write or Full Access to use this tool"}};
    }
    return std::nullopt;
}

json read_file_impl(const json& args, ToolContext& ctx) {
    const std::string path = args.value("path", "");
    if (path.empty()) return json{{"error", "path is required"}};
    std::string err;
    const auto resolved = resolve_workspace_path(ctx, path, true, &err);
    if (!resolved) return json{{"error", err}};

    std::ifstream f(*resolved, std::ios::binary);
    if (!f) return json{{"error", "cannot open file: " + path}};
    std::ostringstream ss;
    ss << f.rdbuf();
    const std::string content = ss.str();
    // Cap at 512KB to avoid flooding the context window.
    if (content.size() > 512 * 1024)
        return json{{"error", "file too large (over 512KB), use a shell tool instead"}};
    return json{{"content", content}, {"size", content.size()}};
}

json write_file_impl(const json& args, ToolContext& ctx) {
    if (auto deny = ws_read_only_deny(ctx, "write_file")) return *deny;
    const std::string path = args.value("path", "");
    const std::string content = args.value("content", "");
    if (path.empty()) return json{{"error", "path is required"}};
    std::string err;
    const auto resolved = resolve_workspace_path(ctx, path, false, &err);
    if (!resolved) return json{{"error", err}};

    std::ofstream f(*resolved, std::ios::binary | std::ios::trunc);
    if (!f) return json{{"error", "cannot write file: " + path}};
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    f.close();
    return json{{"written", true}, {"bytes", content.size()}, {"path", path}};
}

json edit_file_impl(const json& args, ToolContext& ctx) {
    if (auto deny = ws_read_only_deny(ctx, "edit_file")) return *deny;
    const std::string path = args.value("path", "");
    const std::string old_text = args.value("old", "");
    const std::string new_text = args.value("new", "");
    if (path.empty() || old_text.empty())
        return json{{"error", "path and old are required"}};
    std::string err;
    const auto resolved = resolve_workspace_path(ctx, path, true, &err);
    if (!resolved) return json{{"error", err}};

    std::ifstream in(*resolved, std::ios::binary);
    if (!in) return json{{"error", "cannot open file: " + path}};
    std::ostringstream ss;
    ss << in.rdbuf();
    in.close();
    std::string content = ss.str();

    const auto idx = content.find(old_text);
    if (idx == std::string::npos)
        return json{{"error", "old text not found in file"}};
    content.replace(idx, old_text.size(), new_text);

    std::ofstream out(*resolved, std::ios::binary | std::ios::trunc);
    if (!out) return json{{"error", "cannot write file: " + path}};
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    out.close();
    return json{{"edited", true}, {"replaced", 1}, {"path", path}};
}

} // namespace

void register_fs_tools(ToolRegistry& reg) {
    reg.register_tool({
        "read_file",
        "Read a text file from the workspace. Returns content (capped at 512KB).",
        {
            {"type", "object"},
            {"properties", {{"path", {{"type", "string"}}}}},
            {"required", json::array({"path"})}
        },
        true,
        false,
        read_file_impl
    });
    reg.register_tool({
        "write_file",
        "Create or overwrite a text file in the workspace.",
        {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}}},
                {"content", {{"type", "string"}}}
            }},
            {"required", json::array({"path", "content"})}
        },
        false,               // exclusive: file writes serialize
        true,                // requires approval (modifies files)
        write_file_impl
    });
    reg.register_tool({
        "edit_file",
        "Replace the first occurrence of an exact string in a file.",
        {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}}},
                {"old", {{"type", "string"}}},
                {"new", {{"type", "string"}}}
            }},
            {"required", json::array({"path", "old", "new"})}
        },
        false,
        true,                // requires approval (modifies files)
        edit_file_impl
    });
}

} // namespace a