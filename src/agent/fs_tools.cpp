// AriaAgent — filesystem tools implementation.
#include "agent/fs_tools.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <algorithm>
#include <cstdlib>

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
    QFileInfo fi(QString::fromStdString(p));
    const QString root = QDir::cleanPath(QString::fromStdString(kWorkspace));
    const QString full = QDir::cleanPath(fi.absoluteFilePath());
    if (!full.startsWith(root + "/") && full != root) {
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

    QFile f(QString::fromStdString(path));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return json{{"error", "cannot open file: " + path}};
    const QString content = QTextStream(&f).readAll();
    // Cap at 512KB to avoid flooding the context window.
    if (content.size() > 512 * 1024)
        return json{{"error", "file too large (over 512KB), use a shell tool instead"}};
    return json{{"content", content.toStdString()}, {"size", content.size()}};
}

json write_file_impl(const json& args, ToolContext&) {
    if (auto deny = ws_read_only_deny("write_file")) return *deny;
    const std::string path = args.value("path", "");
    const std::string content = args.value("content", "");
    if (path.empty()) return json{{"error", "path is required"}};
    std::string err;
    if (!in_workspace(path, &err)) return json{{"error", err}};

    QFile f(QString::fromStdString(path));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return json{{"error", "cannot write file: " + path}};
    f.write(content.data(), static_cast<qint64>(content.size()));
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

    QFile f(QString::fromStdString(path));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return json{{"error", "cannot open file: " + path}};
    const QString content = QTextStream(&f).readAll();
    f.close();

    const qsizetype idx = content.indexOf(QString::fromStdString(old_text));
    if (idx < 0)
        return json{{"error", "old text not found in file"}};
    QString updated = content;
    updated.replace(idx, static_cast<qsizetype>(old_text.size()), QString::fromStdString(new_text));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return json{{"error", "cannot write file: " + path}};
    f.write(updated.toUtf8());
    f.close();
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
