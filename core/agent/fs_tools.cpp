// AriaAgent — filesystem tools implementation (pure C++, no Qt).
#include "agent/fs_tools.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace agent {

using json = nlohmann::json;

namespace {

constexpr const char* kWorkspace = "D:/Coding/AriaAgent";   // sandbox root

// Read-only mode guard (mirrors shell_tools::ws_read_only_deny).
std::optional<json> ws_read_only_deny(const char* op) {
    const char* mode = std::getenv("ARIA_WORKSPACE_WRITE");
    if (mode && *mode && *mode != '1' && *mode != '2') {
        return json{{"error", op}, {"reason",
            "workspace mode is Read Only — switch the input bar dropdown "
            "to Workspace Write or Full Access to use this tool"}};
    }
    return std::nullopt;
}

// Refuse paths that escape the workspace (path traversal guard).
bool in_workspace(const std::string& p, std::string* err) {
    std::error_code ec;
    std::filesystem::path full = std::filesystem::absolute(p, ec);
    if (ec) full = std::filesystem::path(p);
    std::filesystem::path root = std::filesystem::weakly_canonical(kWorkspace, ec);
    if (ec) root = std::filesystem::path(kWorkspace);
    full = full.lexically_normal();

    const std::string fs = full.string();
    const std::string rs = root.string();
    if (fs != rs && fs.rfind(rs + "/", 0) != 0 && fs.rfind(rs + "\\", 0) != 0) {
        if (err) *err = "path outside workspace root is not allowed: " + p;
        return false;
    }
    return true;
}

json read_file_impl(const json& args, ToolContext&) {
    const std::string path = args.value("path", "");
    if (path.empty()) return json{{"error", "path is required"}};
    std::string err;
    if (!in_workspace(path, &err)) return json{{"error", err}};

    std::ifstream f(path, std::ios::binary);
    if (!f) return json{{"error", "cannot open file: " + path}};
    std::ostringstream ss;
    ss << f.rdbuf();
    const std::string content = ss.str();
    // Cap at 512KB to avoid flooding the context window.
    if (content.size() > 512 * 1024)
        return json{{"error", "file too large (over 512KB), use a shell tool instead"}};
    return json{{"content", content}, {"size", content.size()}};
}

json write_file_impl(const json& args, ToolContext&) {
    if (auto deny = ws_read_only_deny("write_file")) return *deny;
    const std::string path = args.value("path", "");
    const std::string content = args.value("content", "");
    if (path.empty()) return json{{"error", "path is required"}};
    std::string err;
    if (!in_workspace(path, &err)) return json{{"error", err}};

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return json{{"error", "cannot write file: " + path}};
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    f.close();
    return json{{"written", true}, {"bytes", content.size()}, {"path", path}};
}

json edit_file_impl(const json& args, ToolContext&) {
    if (auto deny = ws_read_only_deny("edit_file")) return *deny;
    const std::string path = args.value("path", "");
    const std::string old_text = args.value("old", "");
    const std::string new_text = args.value("new", "");
    if (path.empty() || old_text.empty())
        return json{{"error", "path and old are required"}};
    std::string err;
    if (!in_workspace(path, &err)) return json{{"error", err}};

    std::ifstream in(path, std::ios::binary);
    if (!in) return json{{"error", "cannot open file: " + path}};
    std::ostringstream ss;
    ss << in.rdbuf();
    in.close();
    std::string content = ss.str();

    const auto idx = content.find(old_text);
    if (idx == std::string::npos)
        return json{{"error", "old text not found in file"}};
    content.replace(idx, old_text.size(), new_text);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
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

} // namespace agent
