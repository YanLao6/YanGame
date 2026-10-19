# SubsystemBrowserPlugin — 技术总结

| 项 | 值 |
|---|---|
| 仓库 | https://github.com/aquanox/SubsystemBrowserPlugin |
| Star | 465 · 许可 MIT |
| 最后提交 | 2026-06-26 |

## 这是什么
编辑器面板插件：把当前运行中的**所有 Subsystem**（Engine / Editor / GameInstance / World / LocalPlayer）列成一棵树，并允许**实时查看和修改它们的属性**。

## 核心技术点
- UE 没有内置的 Subsystem 查看器 —— 子系统是"隐形"的，只能靠日志或断点确认状态。这个插件补上了这个空白。
- 支持运行时改属性（相当于给子系统一个 Details 面板），配合 PIE 调试非常高效。
- 可按类别过滤、显示子系统所属模块。

## 与 YanGame 的关系
**本批里对你项目日常调试最实用的工具之一**。你的项目大量用子系统：`UEquipmentDisplaySubsystem`、GameSettings 相关、MessageRouter。这些的运行时状态目前只能靠日志推断，装上这个插件就能直接看。

体积仅 2 MB，MIT，零侵入（纯 Editor 模块），**建议直接装**。

## 注意事项
Editor-only 插件，打包时自动排除，不影响运行时。
