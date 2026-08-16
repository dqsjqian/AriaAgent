<div align="center">

# ✦ AriaAgent

**Industrial-grade C++20 Agent Tooling Framework GUI** · Built on [Aria](https://github.com/dqsjqian/Aria) (C++20 MVVM)

Provider-agnostic · True token-level SSE streaming · Tool-call chain visualization · Permission approval · MIT License

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Qt6](https://img.shields.io/badge/Qt-6-green.svg)](https://www.qt.io)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey.svg)](https://github.com/dqsjqian/Aria)

</div>

> **简体中文** | [English](README.en.md)

---

## What is it?

**AriaAgent** is a **provider-agnostic LLM Agent tooling framework GUI** built on
[Aria](https://github.com/dqsjqian/Aria), an industrial-grade C++20 MVVM framework.

It is not tied to any single model vendor — every **OpenAI-compatible endpoint**
(DeepSeek / OpenAI / Kimi / Qwen / GLM / …) works out of the box. Switching
providers is a one-line configuration change: no code edits, no recompile.

The agent loop (think → call tools → observe → repeat) is written with C++20
coroutines, and the UI layer is fully decoupled from the engine through Aria's
reactive core (`Property` / `ObservableList`). The overall design borrows the
architectural essence of the official DeepSeek harness: event log as the single
source of truth, schema-driven tools, and fail-closed permissions.

## ✨ Features

### 🧠 Agent Core
- **Provider-agnostic** — abstract `LlmClient` interface + `OpenAiCompatClient`
  implementation; any OpenAI-compatible API plugs in seamlessly
- **True streaming** — token-level SSE rendering (cpp-httplib 0.53.1
  `ContentReceiver`), not buffered fake streaming
- **Agent loop** — coroutine think/tool/observe loop with **bounded parallel**
  tool execution (exclusive barrier + parallel pool, results committed in model
  order) and a hard round cap to prevent runaway
- **Tool registry** — plug-and-play: `Tool{name, desc, schema, fn}`, no
  hardcoded branches
- **Arg validation** — lightweight JSON-Schema validator (type/required/enum/
  range) with path-qualified error messages

### 🛠 Built-in tools (10+)
| Tool | Description | Permission |
|---|---|---|
| `calculator` | Arithmetic / power | No approval |
| `current_time` | Current local time | No approval |
| `run_command` | Synchronous shell exec (with timeout) | **Approval** |
| `run_in_background` / `read_output` / `kill_process` | Background process handle + incremental polling | **Approval** |
| `read_file` / `write_file` / `edit_file` | File read/write/edit (path-traversal guarded) | **Write = Approval** |
| `list_directory` | Directory listing | No approval |
| `todo_set` / `todo_add` / `todo_list` | Agent-visible todos (snapshot last-wins) | No approval |

### 🗂 Session & UI
- **Multi-session** — sidebar session list (create / switch / right-click delete),
  JSON persistence to `~/.ariaagent/sessions/`, auto-restore on launch
- **Multi-turn context** — the engine owns the full message history; the agent
  has memory
- **Auto-compaction** — summarizes old turns past 32 messages without splitting
  tool-call/result pairs
- **Markdown rendering** — in-bubble Markdown + 4-color syntax highlighting
- **Trajectory panel** — right-side tool-call timeline (success/failure colors)
- **Todo panel** — live projection of the agent's todo list
- **Message feedback** — right-click 👍/👎, persisted
- **Approval gate** — modal confirmation before dangerous tools, default-deny
  (fail-closed)

### 🏗 Architecture (pure-C++ core + platform shells)
```
AriaAgent/
├── core/                    # ★ Pure C++, zero Qt (reused verbatim on mobile)
│   ├── agent/               #   engine: agent loop / llm_client / tools /
│   │                        #   session_store / json_schema / subprocess
│   └── module_api/          #   BaseVm / IModule / ModuleRegistry / ServiceHub
├── modules/                 # ★ feature modules (plugin pattern, VM per module)
│   ├── chat/                #   chat module (engine bridge + ChatViewModel)
│   ├── sessions/            #   session list (sidebar projection)
│   ├── settings/            #   settings + Qt settings dialog
│   ├── todo/  trajectory/   #   todos / tool-call trajectory
│   └── app/                 #   app shell
│       ├── viewmodel/       #   AppText (UI string service)
│       └── platforms/qt/    #   ★ Shell: main.cpp (QtDispatcher) /
│                            #     main_window / markdown_render
├── third_party/aria         # vendored framework (submodule)
├── core/CMakeLists.txt      # ariaagent_core
└── modules/app/platforms/qt/CMakeLists.txt  # aria_agent executable
```

> Note: the legacy `platform/` directory is dead code; the real Qt shell lives
> in `modules/app/platforms/qt/` (see the top-level CMakeLists.txt). Future
> iOS/Android shells will live under `modules/app/platforms/` and link the same
> core verbatim.
**Threading**: the VM marshals back to the UI thread via
`aria::runtime::main_dispatcher()` — the Qt shell installs a `QtDispatcher`,
mobile shells install their own; the VM never knows which platform it is on.

**Approval prompts**: the VM exposes an `approval_ui` callback that the shell
injects (QMessageBox / UIAlertController / Android Dialog). The VM never pops
a dialog itself.

## 🚀 Quick start

### Prerequisites
- **Windows**: MSYS2 UCRT64 (GCC 13+), Qt6, OpenSSL, CMake ≥ 3.20
- **macOS**: Xcode CommandLineTools, Qt6 (`brew install qt`), CMake ≥ 3.20
- Initialize the Aria submodule first (do **not** use `--recursive` — the
  vendored openssl submodule carries 10 test-only submodules that take forever
  to clone):

```bash
git submodule update --init
```

### Build (macOS)

```bash
cmake -S . -B build/flavors/debug -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build/flavors/debug -j 8
./build/flavors/debug/bin/aria_agent
```

### Build (Windows)

```powershell
# MSYS2 toolchain
export PATH="/d/worksoft/msys64/ucrt64/bin:$PATH"

cmake -S . -B build/flavors/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug `
      -DCMAKE_PREFIX_PATH="D:/worksoft/msys64/ucrt64"
cmake --build build/flavors/debug -j 8

# Deploy runtime DLLs (windeployqt + recursive closure copy; double-click ready)
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/deploy-dlls.ps1
```

### Configure & run

Use the built-in settings dialog (⚙ bottom-left) or environment variables:

| Variable | Description | Default |
|---|---|---|
| `ARIA_LLM_API_KEY` | API key | — |
| `ARIA_LLM_BASE_URL` | OpenAI-compatible endpoint | `https://api.deepseek.com` |
| `ARIA_LLM_MODEL` | Model name | `deepseek-chat` |
| `ARIA_LLM_SYSTEM_PROMPT` | System prompt | default assistant prompt |

```powershell
$env:ARIA_LLM_API_KEY  = "sk-..."
$env:ARIA_LLM_BASE_URL = "https://api.deepseek.com"
$env:ARIA_LLM_MODEL    = "deepseek-chat"
./build/flavors/debug/aria_agent.exe
```

> Switch providers: point `BASE_URL` at `https://api.openai.com` + `gpt-4o-mini`,
> or `https://api.moonshot.cn` + `kimi-k2-0711-preview` — no recompile needed.

### Release build

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-release.ps1
```

## 🧩 Extending

### Registering a new tool (one line)

```cpp
reg.register_tool({
    "web_search",                            // name
    "Search the web for a query.",           // description
    {                                        // JSON Schema parameters
        {"type", "object"},
        {"properties", {{"query", {{"type", "string"}}}}},
        {"required", json::array({"query"})}
    },
    /*concurrency_safe=*/true,               // may run in parallel
    /*requires_approval=*/true,              // needs user approval
    web_search_impl                          // implementation fn
});
```

### Switching model vendors
See the env table above. `ARIA_LLM_BASE_URL` + `ARIA_LLM_MODEL` is all you
need — the `LlmClient` abstraction guarantees zero code changes.

## 📚 Design provenance

The architecture draws heavily on the core ideas of the official
[deepseek-ai/deepseek-harness](https://github.com/deepseek-ai/deepseek-harness):

1. **Event log = single source of truth** — sessions can be replayed,
   compacted, and synchronized across views
2. **Tool = schema + fn** — plug-and-play registration
3. **UI never touches the engine directly** — only reactive state in between
4. **Fail-closed permissions** — dangerous operations need explicit approval
5. **Replayable** — any state can be rebuilt from the log

## 📄 License

[MIT](LICENSE) © 2026 dqsjqian
