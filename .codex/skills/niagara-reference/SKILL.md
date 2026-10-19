---
name: niagara-reference
description: Index of Niagara VFX reference documents. Provides quick-reference sim rules, particle budgets, and gotchas. Points to detailed docs on demand. Use when creating, optimizing, or debugging Niagara effects.
type: index
tags: [monolith, unreal]
requires: []
---

# niagara-reference（Monolith 路由）

本文件只做路由，不复制正文。权威内容随 Monolith 插件版本更新，位于：

`Plugins/Monolith/Skills/niagara-reference/niagara-reference.md`

## 使用步骤

1. 先完整阅读上述文件（含其相对路径引用的 reference 文档，路径以该文件所在目录为基准）。
2. 调用任何 action 前先 `monolith_discover("<namespace>")` 确认 action 名，参数不确定时用 `describe_query("action_schema", ...)`，不要猜。
3. 写操作完成后按该文档给出的 verify 调用做回读验证，只信读回的结果，不信写调用的 success 标志。

## 边界

- Monolith 只在编辑器运行时可用；`monolith_status()` 失败即编辑器未启动或已崩溃。
- 编辑器关闭时只读查询可退回 `Plugins/Monolith/Binaries/monolith_query.exe`。
- 本项目还挂有 `unreal-yangame` MCP（8010 端口，另一套工具集）；两者能力重叠时优先用当前任务已在用的那一套，避免同一资产被两条链路交叉写。
