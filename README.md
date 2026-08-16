# AriaAgent

**LLM Agent 工具框架 GUI** —— 基于 [Aria](https://github.com/dqsjqian/Aria)(工业级 C++20 MVVM 框架)。

一个 provider 无关的 Agent 桌面应用:内置工具注册/执行机制,Agent 循环(思考 → 调工具 → 观察 → 再思考),聊天界面 + 工具调用链可视化,true token-by-token 流式输出。

## 特性

- **Provider 无关** —— 任何 OpenAI 兼容 API 都能接(DeepSeek / OpenAI / Kimi / Qwen / GLM …),通过环境变量切换,零代码改动
- **真流式输出** —— token 级 SSE 流式渲染,不是缓冲式假流式
- **Agent 循环** —— 协程式思考/工具调用/观察,支持多工具并行调用,硬性轮数上限防失控
- **工具注册表** —— 内置 `calculator`、`current_time`,新增工具只需注册一个 `Tool{name, desc, schema, fn}`
- **响应式 UI** —— 基于 Aria `ObservableList` / `Property`,Qt6 适配器声明式绑定,工具调用链实时时间线
- **取消支持** —— 运行中可停止

## 构建

前置:CMake ≥ 3.20,MSYS2 UCRT64(GCC),Qt6,OpenSSL。

```powershell
# 1. 拉取 Aria 子模块
git submodule update --init --recursive

# 2. 配置 + 构建
cmake -S . -B build/flavors/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=D:/worksoft/msys64/ucrt64
cmake --build build/flavors/debug

# 3. 复制运行时 DLL(aria + Qt + MinGW + OpenSSL)到 exe 目录
#    (参照 examples/1-qt-showcase 的 Copy-Dependencies 逻辑)
```

## 配置(环境变量)

| 变量 | 说明 | 默认值 |
|---|---|---|
| `ARIA_LLM_API_KEY` | API 密钥(通用) | — |
| `ARIA_LLM_BASE_URL` | 端点地址 | `https://api.deepseek.com` |
| `ARIA_LLM_MODEL` | 模型名 | `deepseek-chat` |

兼容回退:`DEEPSEEK_API_KEY` / `OPENAI_API_KEY`(按 base_url 自动识别)。

```powershell
$env:ARIA_LLM_API_KEY  = "sk-..."
$env:ARIA_LLM_BASE_URL = "https://api.deepseek.com"
$env:ARIA_LLM_MODEL    = "deepseek-chat"
./build/flavors/debug/aria_agent.exe
```

换一家:改成 `https://api.openai.com` + `gpt-4o-mini`,或 `https://api.moonshot.cn` + `kimi-k2-0711-preview`,无需重新编译。

## 项目结构

```
src/
├── agent/                  # 引擎核心(UI/Provider 无关)
│   ├── model.hpp           #   消息/工具调用/阶段领域模型
│   ├── llm_client.hpp/cpp  #   LlmClient 抽象 + OpenAI 兼容实现 + 工厂
│   ├── agent.hpp/cpp       #   Agent 循环(思考→工具→观察)
│   └── tool_registry.hpp/cpp # 工具注册/执行 + 内置工具
└── ui/                     # Qt6 GUI 层
    ├── chat_view_model.hpp/cpp # 响应式 ViewModel
    └── main_window.hpp/cpp     # 主窗口 + 列表绑定
```

## 架构

```
LlmClient (OpenAI-compatible, 可换 provider)
      │
AgentEngine ── 循环: complete_stream → tool_calls? → execute → 回灌
      │
  callbacks(线程无关)
      │
ChatViewModel (aria ObservableList/Property — UI 唯一数据源)
      │
Qt6 adapter → 聊天列表 / 工具链时间线 / 输入栏
```

## License

MIT
