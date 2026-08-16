<div align="center">

# ✦ AriaAgent

**工业级 C++20 Agent 工具框架 GUI** · 基于 [Aria](https://github.com/dqsjqian/Aria) (C++20 MVVM)

Provider 无关 · 真流式 SSE · 工具调用链可视化 · 权限审批 · MIT License

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Qt6](https://img.shields.io/badge/Qt-6-green.svg)](https://www.qt.io)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey.svg)](https://github.com/dqsjqian/Aria)

</div>

---

## 这是什么？

**AriaAgent** 是一个基于 Aria(C++20 MVVM 框架)构建的 **provider 无关 LLM Agent 工具框架 GUI**。
它不绑定任何一家模型厂商 —— DeepSeek / OpenAI / Kimi / Qwen / GLM 等所有 **OpenAI 兼容端点**开箱即用,换模型只需改一行配置,零代码改动、无需重新编译。

Agent 循环(思考 → 调工具 → 观察 → 再思考)用 C++20 协程实现,UI 层通过 Aria 的响应式引擎(Property / ObservableList)与引擎层彻底解耦。整体设计大量借鉴 DeepSeek 官方 harness 的架构精髓(事件日志 = 唯一事实源、工具 schema 驱动、权限默认拒绝)。

## ✨ 特性

### 🧠 Agent 核心
- **Provider 无关** —— 抽象 `LlmClient` 接口 + `OpenAiCompatClient` 实现,任何 OpenAI 兼容 API 无缝接入
- **真流式输出** —— token 级 SSE 流式渲染(cpp-httplib 0.53.1 `ContentReceiver`),不是缓冲式假流式
- **Agent 循环** —— 协程式 思考/工具调用/观察 循环,多工具**有界并行**执行(exclusive 屏障 + 并行池,结果按模型顺序提交),硬性轮数上限防失控
- **工具注册表** —— 一次注册即插即用:`Tool{name, desc, schema, fn}`,无硬编码分支
- **参数校验** —— 轻量 JSON-Schema 校验器(类型/必填/枚举/范围),错误信息带 JSON 路径

### 🛠 内置工具(10+)
| 工具 | 说明 | 权限 |
|---|---|---|
| `calculator` | 四则/幂运算 | 无需审批 |
| `current_time` | 当前本地时间 | 无需审批 |
| `run_command` | 同步执行 Shell 命令(超时) | **需审批** |
| `run_in_background` / `read_output` / `kill_process` | 后台进程句柄 + 增量轮询 | **需审批** |
| `read_file` / `write_file` / `edit_file` | 文件读写改(防目录逃逸) | **写操作需审批** |
| `list_directory` | 目录列表 | 无需审批 |
| `todo_set` / `todo_add` / `todo_list` | Agent 可见待办(快照 last-wins) | 无需审批 |

### 🗂 会话与 UI
- **多会话管理** —— 侧边栏会话列表(新建/切换/右键删除),JSON 持久化到 `~/.ariaagent/sessions/`,重启自动恢复
- **多轮上下文** —— 引擎持有完整消息历史,Agent 有记忆
- **自动压缩** —— 超 32 条自动摘要压缩,不拆散 tool-call/result 配对
- **Markdown 渲染** —— 气泡内渲染 Markdown + 代码四色高亮
- **轨迹面板** —— 右侧工具调用时间线(成功/失败着色)
- **Todo 面板** —— Agent 的待办列表实时投影
- **消息反馈** —— 右键 👍/👎,持久化
- **权限审批** —— 危险工具执行前模态确认,默认拒绝(fail-closed)

### 🏗 架构分层
```
src/
├── agent/            # 引擎层(零 UI 依赖、零 provider 依赖)
│   ├── agent.cpp     #   协程 Agent 循环
│   ├── llm_client.*  #   LlmClient 接口 + OpenAI 兼容实现
│   ├── tool_registry.*  #   工具注册 + JSON-Schema 校验
│   ├── shell_tools.* / fs_tools.* / todo_tools.*
│   ├── session_store.*  #   会话持久化
│   └── model.hpp / json_schema.*
├── ui/               # Qt6 界面层(DeepSeek harness 设计语言)
│   ├── chat_view_model.*  #   响应式 ViewModel
│   ├── main_window.*      #   主窗口 + 气泡 delegate
│   ├── settings_dialog.*  #   设置(通用/模型/插件/预设)
│   └── markdown_render.*  #   Markdown → HTML 渲染器
└── main.cpp
```

## 🚀 快速开始

### 前置
- **Windows**:MSYS2 UCRT64(GCC 13+)、Qt6、OpenSSL、CMake ≥ 3.20
- Aria 子模块需先初始化:

```bash
git submodule update --init --recursive
```

### 构建(Windows)

```powershell
# MSYS2 工具链
export PATH="/d/worksoft/msys64/ucrt64/bin:$PATH"

cmake -S . -B build/flavors/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug `
      -DCMAKE_PREFIX_PATH="D:/worksoft/msys64/ucrt64"
cmake --build build/flavors/debug -j 8

# 部署运行时 DLL(windeployqt + 递归依赖拷贝,双击即可运行)
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/deploy-dlls.ps1
```

### 配置 & 运行

应用内置设置对话框(左下角 ⚙),或直接使用环境变量:

| 变量 | 说明 | 默认值 |
|---|---|---|
| `ARIA_LLM_API_KEY` | API 密钥 | — |
| `ARIA_LLM_BASE_URL` | OpenAI 兼容端点 | `https://api.deepseek.com` |
| `ARIA_LLM_MODEL` | 模型名 | `deepseek-chat` |
| `ARIA_LLM_SYSTEM_PROMPT` | 系统提示词 | 默认助手提示 |

```powershell
$env:ARIA_LLM_API_KEY  = "sk-..."
$env:ARIA_LLM_BASE_URL = "https://api.deepseek.com"
$env:ARIA_LLM_MODEL    = "deepseek-chat"
./build/flavors/debug/aria_agent.exe
```

> 换厂商:把 `BASE_URL` 改成 `https://api.openai.com` + `gpt-4o-mini`,或 `https://api.moonshot.cn` + `kimi-k2-0711-preview`,无需重新编译。

### 发布构建

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-release.ps1
```

## 🧩 扩展

### 添加一个新工具(注册一行)

```cpp
reg.register_tool({
    "web_search",                            // 工具名
    "Search the web for a query.",           // 描述
    {                                        // JSON Schema 参数
        {"type", "object"},
        {"properties", {{"query", {{"type", "string"}}}}},
        {"required", json::array({"query"})}
    },
    /*concurrency_safe=*/true,               // 可并行
    /*requires_approval=*/true,              // 需审批
    web_search_impl                          // 实现函数
});
```

### 切换模型厂商
见上方环境变量表。`ARIA_LLM_BASE_URL` + `ARIA_LLM_MODEL` 即可,`LlmClient` 抽象层保证零代码改动。

## 📚 设计来源

架构设计大量参考 DeepSeek 官方开源 harness([deepseek-ai/deepseek-harness](https://github.com/deepseek-ai/deepseek-harness))的核心思想:

1. **事件日志 = 唯一事实源** —— 会话可回放、可压缩、可多视图同步
2. **工具 = schema + fn** —— 注册即插即用
3. **UI 永不直接碰引擎** —— 中间只有响应式状态
4. **权限默认拒绝** —— 危险操作必须显式审批
5. **可回放** —— 任何状态都能从日志重建

## 📄 License

[MIT](LICENSE) © 2026 dqsjqian
