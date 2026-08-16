// AriaAgent — shell / subprocess tools implementation.
#include "agent/shell_tools.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryFile>
#include <QTextStream>

#include <chrono>
#include <map>
#include <mutex>

namespace agent {

using json = nlohmann::json;

namespace {

constexpr const char* kWorkspace = "D:/Coding/AriaAgent";   // default sandbox root

// ── Background process registry (handle → QProcess*) ───────────────────────
struct ProcRegistry {
    std::mutex mu;
    std::map<int, QProcess*> procs;
    int next_handle{1};
};
ProcRegistry& procs() {
    static ProcRegistry r;
    return r;
}

json run_command_impl(const json& args, ToolContext&) {
    const std::string cmd = args.value("command", "");
    const int timeout_ms = args.value("timeout_ms", 30000);
    if (cmd.empty()) return json{{"error", "command is required"}};

    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    auto t0 = std::chrono::steady_clock::now();
    p.start(QString::fromStdString(cmd));
    if (!p.waitForStarted(5000))
        return json{{"error", "failed to start command"}};
    if (!p.waitForFinished(timeout_ms)) {
        p.kill();
        p.waitForFinished(3000);
        auto t1 = std::chrono::steady_clock::now();
        return json{
            {"exit_code", -1},
            {"timed_out", true},
            {"output", p.readAll().toStdString()},
            {"duration_ms", std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()},
        };
    }
    auto t1 = std::chrono::steady_clock::now();
    return json{
        {"exit_code", p.exitCode()},
        {"output", p.readAll().toStdString()},
        {"duration_ms", std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()},
    };
}

json run_background_impl(const json& args, ToolContext&) {
    const std::string cmd = args.value("command", "");
    if (cmd.empty()) return json{{"error", "command is required"}};

    auto* p = new QProcess;
    p->setProcessChannelMode(QProcess::MergedChannels);
    p->start(QString::fromStdString(cmd));
    if (!p->waitForStarted(5000)) {
        delete p;
        return json{{"error", "failed to start command"}};
    }
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
    QProcess* p = it->second;
    const QByteArray out = p->readAll();
    return json{
        {"output", out.toStdString()},
        {"running", p->state() == QProcess::Running},
        {"exit_code", p->state() == QProcess::NotRunning ? p->exitCode() : 0},
    };
}

json kill_process_impl(const json& args, ToolContext&) {
    const int handle = args.value("handle", -1);
    std::lock_guard<std::mutex> lk(procs().mu);
    auto it = procs().procs.find(handle);
    if (it == procs().procs.end())
        return json{{"error", "unknown process handle"}};
    QProcess* p = it->second;
    if (p->state() == QProcess::Running) p->kill();
    p->waitForFinished(3000);
    p->deleteLater();
    procs().procs.erase(it);
    return json{{"killed", true}};
}

json list_directory_impl(const json& args, ToolContext&) {
    const std::string path = args.value("path", kWorkspace);
    QDir dir(QString::fromStdString(path));
    if (!dir.exists()) return json{{"error", "directory does not exist: " + path}};

    json items = json::array();
    const auto entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries,
                                           QDir::Name | QDir::DirsFirst);
    for (const auto& e : entries) {
        items.push_back({
            {"name", e.fileName().toStdString()},
            {"type", e.isDir() ? "dir" : "file"},
            {"size", e.isFile() ? e.size() : 0},
        });
    }
    return json{{"items", items}, {"count", items.size()}};
}

} // namespace

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
