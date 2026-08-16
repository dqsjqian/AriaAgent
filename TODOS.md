# AriaAgent — Roadmap (ported from deepseek-harness)

> 移植自 deepseek-harness (https://github.com/deepseek-ai/deepseek-harness)
> 优先级按「价值 × 成本 × 依赖顺序」排列。每项完成后勾选。

## P0 — 基础架构(先做,后续都依赖)

- [x] **P0-1 项目骨架**: CMake + Aria submodule + Qt6 + OpenAI 兼容 LLM 客户端 + 真流式 SSE
- [x] **P0-2 DeepSeek 风格 UI**: 侧边栏 + 气泡聊天 + 输入栏 + 设置对话框
- [x] **P0-3 DLL 部署脚本**: windeployqt + 递归依赖拷贝 (scripts/deploy-dlls.ps1)
- [x] **P0-4 会话持久化 + 多会话管理** (session + ui-sidebar 移植)
      - 追加式事件日志 = 唯一事实源 (seq 序列号)
      - 多会话列表: 新建 / 切换 / 删除, JSON 持久化
      - 重启后自动恢复上次会话
      - 参考: harness packages/core/session, packages/client/ui-sidebar

## P1 — 工具系统(让 agent 真正干活)

- [x] **P1-1 工具 Schema DSL + 校验** (tools/schema 移植)
      - 一次定义 → 三重产出: 参数校验 + 类型 + UI 提示
      - 路径化校验错误 (ToolArgsError)
      - 参考: harness packages/core/tools/src/schema.ts, json-schema.ts
- [x] **P1-2 工具执行调度** (tool-calls 移植)
      - exclusive 屏障 (串行) + 有界并行池 (std::thread, max 4)
      - 结果按模型顺序提交
      - 参考: harness packages/core/agent-loop/src/tool-calls.ts
- [x] **P1-3 Shell/子进程工具** (shell/subprocess 移植)
      - QProcess 封装: run_command / run_in_background / read_output / kill_process
      - 参考: harness packages/subprocess, packages/shell
- [x] **P1-4 文件系统工具** (fs 移植)
      - read_file / write_file / edit_file / list_directory
      - 路径白名单 (workspace-write 语义, 防目录逃逸)
      - 参考: harness packages/fs

## P2 — 会话增强(体验补完)

- [x] **P2-1 Markdown 渲染 + 代码高亮** (ui-conversation 移植)
      - 聊天气泡内渲染 Markdown, 代码块带语法高亮 (四色)
      - 参考: harness packages/client/ui-conversation
- [x] **P2-2 对话压缩** (compaction 移植)
      - 摘要替换旧区间, 不拆散 tool-call/result 配对, 保留最近 8 条
      - 超 32 条自动触发
      - 参考: harness packages/compaction
- [x] **P2-3 会话轨迹回放视图** (ui-trajectory 移植)
      - 右侧时间线: 工具调用节点 (成功/失败着色), 结果截断
      - 参考: harness packages/client/ui-trajectory
- [x] **P2-4 消息反馈** (feedback 移植)
      - 右键助手消息 👍 / 👎, 按内容 hash 持久化到 QSettings
      - 参考: harness packages/feedback

## P3 — Agent 智能(进阶)

- [x] **P3-1 todo 快照投影** (todo 移植)
      - 全量快照 last-wins + 观察者, Agent 可见待办 (todo_set/add/list)
      - UI 右侧 ✅ Todo 面板实时投影
      - 参考: harness packages/todo
- [x] **P3-2 权限审批链** (sandbox/escalation 移植)
      - 危险工具 (run_command/write_file/edit_file/…) 标记 requires_approval
      - 执行前模态审批, 默认拒绝 (fail-closed)
      - 参考: harness packages/sandbox
- [ ] **P3-3 定时任务 / 调度** (schedule 移植)
      - 参考: harness packages/schedule
- [ ] **P3-4 LSP 集成** (lsp 移植)
      - 4 操作闭集: goToDefinition / findReferences / goToImplementation / hover
      - 参考: harness packages/lsp

## P4 — 发布打磨

- [ ] **P4-1 设置页补完**: 主题切换 (light/dark) + 插件启停 + 预设管理
- [ ] **P4-2 图标 & 视觉打磨**: 线性图标集, 动画过渡
- [x] **P4-3 打包发布**: scripts/build-release.ps1 (Release 构建 + DLL 部署一步到位)

---

## 架构原则 (来自 harness 的设计精髓)

1. **事件日志 = 唯一事实源**: 所有 UI/压缩/回放都从日志派生, 不做第二份状态
2. **工具 = schema + fn**: 注册即插即用, 无硬编码分支
3. **UI 永不直接碰引擎**: 中间只有响应式状态 (aria ObservableList/Property)
4. **权限默认拒绝**: 危险操作必须显式审批
5. **可回放**: 任何对话状态都能从日志重建 (为未来多端同步留口)
