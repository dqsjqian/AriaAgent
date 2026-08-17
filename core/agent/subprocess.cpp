// AriaAgent — cross-platform subprocess implementation (no Qt).
#include "agent/subprocess.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace agent {

// ── POSIX helpers ───────────────────────────────────────────────────────────
#ifndef _WIN32

static int platform_kill(pid_t pid) {
    return ::kill(pid, SIGKILL);
}

// ── Windows helpers ─────────────────────────────────────────────────────────
#else

// Build a quoted command line: "cmd.exe /C <command>" for system shell.
static std::wstring to_wide(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                      static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n > 0 ? n : 0, L'\0');
    if (n > 0) {
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                            w.data(), n);
    }
    return w;
}

#endif // _WIN32

// ═══════════════════════════════════════════════════════════════════════════
//  Synchronous run (with timeout)
// ═══════════════════════════════════════════════════════════════════════════
ProcResult run_sync(const std::string& command, int timeout_ms,
                    const std::string& working_directory) {
    ProcResult out;

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        out.exit_code = -1;
        out.output = "error: CreatePipe failed";
        return out;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    PROCESS_INFORMATION pi{};
    std::wstring cmdline = L"cmd.exe /C " + to_wide(command);
    std::wstring mutable_cmd = cmdline;   // CreateProcessW may write to it
    const std::wstring working_dir = to_wide(working_directory);
    if (!CreateProcessW(nullptr, mutable_cmd.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr,
                        working_dir.empty() ? nullptr : working_dir.c_str(), &si, &pi)) {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        out.exit_code = -1;
        out.output = "error: CreateProcess failed";
        return out;
    }
    CloseHandle(hWrite);

    auto t0 = std::chrono::steady_clock::now();
    // Read output (bounded by timeout) using a background thread so a
    // long-running child that fills the pipe does not deadlock the reader.
    std::string accumulated;
    std::atomic<bool> done{false};
    std::thread reader([&] {
        char buf[4096];
        DWORD n = 0;
        while (ReadFile(hRead, buf, sizeof(buf), &n, nullptr) && n > 0) {
            accumulated.append(buf, n);
        }
        done = true;
    });

    bool timed_out = false;
    if (timeout_ms > 0) {
        while (true) {
            if (WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0) break;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - t0).count() >= timeout_ms) {
                timed_out = true;
                TerminateProcess(pi.hProcess, 1);
                break;
            }
        }
    } else {
        WaitForSingleObject(pi.hProcess, INFINITE);
    }

    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    WaitForSingleObject(pi.hProcess, 1000);   // let the reader drain
    reader.join();
    CloseHandle(hRead);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    out.exit_code = static_cast<int>(code);
    out.timed_out = timed_out;
    out.output = accumulated;
#else
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        out.exit_code = -1;
        out.output = "error: pipe failed";
        return out;
    }
    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        out.exit_code = -1;
        out.output = "error: fork failed";
        return out;
    }
    if (pid == 0) {
        // child
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        if (!working_directory.empty() && chdir(working_directory.c_str()) != 0) _exit(126);
        execl("/bin/sh", "sh", "-c", command.c_str(), (char*)nullptr);
        _exit(127);
    }
    close(pipefd[1]);

    std::string accumulated;
    std::thread reader([&] {
        char buf[4096];
        ssize_t n;
        while ((n = read(pipefd[0], buf, sizeof(buf))) > 0)
            accumulated.append(buf, n);
    });

    auto t0 = std::chrono::steady_clock::now();
    bool timed_out = false;
    int status = 0;
    if (timeout_ms > 0) {
        while (true) {
            pid_t r = waitpid(pid, &status, WNOHANG);
            if (r == pid) break;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - t0).count() >= timeout_ms) {
                timed_out = true;
                platform_kill(pid);
                waitpid(pid, &status, 0);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    } else {
        waitpid(pid, &status, 0);
    }
    close(pipefd[0]);
    reader.join();

    out.exit_code = timed_out ? -1 : (WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    out.timed_out = timed_out;
    out.output = accumulated;
#endif
    return out;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Background processes
// ═══════════════════════════════════════════════════════════════════════════
struct BgProc {
#ifdef _WIN32
    HANDLE process{nullptr};
    HANDLE read_pipe{nullptr};
#else
    pid_t pid{0};
    int pipe_fd{-1};
#endif
    std::mutex mu;
    std::string buffer;
    std::atomic<bool> running{true};
    std::atomic<int> exit_code{0};
    std::thread reader;
    std::atomic<bool> closed{false};
};

static void bg_reader(BgProc* p) {
#ifdef _WIN32
    char buf[4096];
    DWORD n = 0;
    while (ReadFile(p->read_pipe, buf, sizeof(buf), &n, nullptr) && n > 0) {
        std::lock_guard<std::mutex> lk(p->mu);
        p->buffer.append(buf, n);
    }
#else
    char buf[4096];
    ssize_t n;
    while ((n = read(p->pipe_fd, buf, sizeof(buf))) > 0) {
        std::lock_guard<std::mutex> lk(p->mu);
        p->buffer.append(buf, n);
    }
#endif
    p->running = false;
}

BgProc* bg_start(const std::string& command,
                 const std::string& working_directory) {
    auto* p = new BgProc;
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE hWrite = nullptr;
    if (!CreatePipe(&p->read_pipe, &hWrite, &sa, 0)) { delete p; return nullptr; }
    SetHandleInformation(p->read_pipe, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    PROCESS_INFORMATION pi{};
    std::wstring cmdline = L"cmd.exe /C " + to_wide(command);
    std::wstring mutable_cmd = cmdline;
    const std::wstring working_dir = to_wide(working_directory);
    if (!CreateProcessW(nullptr, mutable_cmd.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr,
                        working_dir.empty() ? nullptr : working_dir.c_str(), &si, &pi)) {
        CloseHandle(p->read_pipe);
        CloseHandle(hWrite);
        delete p;
        return nullptr;
    }
    CloseHandle(hWrite);
    p->process = pi.hProcess;
    CloseHandle(pi.hThread);
#else
    int pipefd[2];
    if (pipe(pipefd) != 0) { delete p; return nullptr; }
    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]); close(pipefd[1]);
        delete p;
        return nullptr;
    }
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        if (!working_directory.empty() && chdir(working_directory.c_str()) != 0) _exit(126);
        execl("/bin/sh", "sh", "-c", command.c_str(), (char*)nullptr);
        _exit(127);
    }
    close(pipefd[1]);
    p->pid = pid;
    p->pipe_fd = pipefd[0];
#endif
    p->reader = std::thread([p] { bg_reader(p); });
    return p;
}

std::string bg_read(BgProc* p) {
    std::lock_guard<std::mutex> lk(p->mu);
    std::string out = std::move(p->buffer);
    p->buffer.clear();
    return out;
}

bool bg_running(BgProc* p) {
    if (!p->running.load()) return false;
#ifdef _WIN32
    return WaitForSingleObject(p->process, 0) == WAIT_TIMEOUT;
#else
    int status = 0;
    pid_t r = waitpid(p->pid, &status, WNOHANG);
    if (r == p->pid) { p->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1; p->running = false; return false; }
    return true;
#endif
}

int bg_exit_code(BgProc* p) {
    return p->exit_code.load();
}

bool bg_kill(BgProc* p) {
    if (!bg_running(p)) return true;
#ifdef _WIN32
    return TerminateProcess(p->process, 1) != 0;
#else
    return platform_kill(p->pid) == 0;
#endif
}

void bg_close(BgProc* p) {
    if (!p) return;
    if (p->closed.exchange(true)) return;
    if (bg_running(p)) bg_kill(p);
#ifdef _WIN32
    CloseHandle(p->process);
    CloseHandle(p->read_pipe);
#else
    close(p->pipe_fd);
#endif
    if (p->reader.joinable()) p->reader.join();
    delete p;
}

} // namespace agent
