# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> 本插件受父级 `CLAUDE.md`（位于 `YanGame/CLAUDE.md`）约束，所有通用规则均继承自该文件。本文件仅记录插件特有的信息。

---

## 构建与编译

本插件为 UE 5.7 插件，仅支持 Win64，依赖以下 UE 内置插件（需在 `.uplugin` 中确认已启用）：
- `GeometryScripting`、`ProceduralMeshComponent`、`Fracture`、`ChaosEditor`、`GeometryCollectionPlugin`

**从命令行编译**（需在 UE 引擎根目录执行）：

```powershell
# 编译 Editor 目标（含 RealtimeDestructionEditor 模块）
.\Engine\Build\BatchFiles\Build.bat YanGameEditor Win64 Development "E:\UnrealEngine\Projecties\YanGame\YanGame.uproject"

# 编译 Game 目标（仅 RealtimeDestruction 运行时模块）
.\Engine\Build\BatchFiles\Build.bat YanGame Win64 Development "E:\UnrealEngine\Projecties\YanGame\YanGame.uproject"
```

插件内无独立测试框架；验证须在编辑器中打开 Demo 关卡（`Content/Demo/Lvl_RDM_Demo`）进行。

---

## 架构总览

### 模块划分

| 模块 | 类型 | 职责 |
|------|------|------|
| `RealtimeDestruction` | Runtime | 运行时核心：破坏逻辑、网络、线程、数据资产 |
| `RealtimeDestructionEditor` | Editor | 编辑器工具：Anchor 编辑模式、Detail 面板、弹孔资产编辑器、组件可视化器 |

### 核心类层次

```
URealtimeDestructibleMeshComponent          ← 主入口（继承 UDynamicMeshComponent）
│  包含：网格 Chunk 管理、网络 RPC、调试可视化、弹孔聚合、Late Join 同步
│
├── FRealtimeBooleanProcessor               ← 异步布尔运算（非 UObject，纯 C++）
│     • Union 阶段（合并多次撞击工具网格）→ Subtract 阶段（从 Chunk 中切除）
│     • MPSC 无锁队列 + URDMThreadManagerSubsystem 线程池
│     • 自适应 Simplify + HC 拉普拉斯平滑（防三角面爆炸）
│
├── FStructuralIntegritySystem              ← 结构完整性（纯 C++，FRWLock 线程安全）
│     • 基于 BFS 的连通性检测：Anchor Cell → 孤立岛 → 生成 ADebrisActor
│     • 层次化 BFS：SuperCell (4×4×4) 快速跳过 → Cell 精细遍历 → SubCell (2×2×2) 精度
│
├── FGridCellLayout                         ← 体素网格（编辑器预计算，运行时只读）
│     • 稀疏数组 + 位域存储 Cell 三角面归属与邻居关系
│     • 警告：运行时修改 WorldScale 会导致网格不匹配，无自动保护
│
├── ADebrisActor                            ← 碎片 Actor
│     • 渲染：ProceduralMeshComponent（注意：主系统用 DynamicMeshComponent，技术栈不统一）
│     • 物理：BoxComponent 作根节点
│     • 同步：位图压缩 CellId 网络复制，降级回退路径为 GenerateMeshFromCells()
│
├── UDestructionNetworkComponent            ← 网络转发组件（挂载到 PlayerController）
│     • ServerApplyDestruction_Compact RPC：FCompactDestructionOp（约 102 bits/请求）
│     • 服务端安全校验：距离、频率、视线、半径上限
│
├── UDestructionGameInstanceSubsystem       ← 服务端 Op 历史（最多 10000 条，Late Join 用）
│
└── URDMThreadManagerSubsystem             ← 全局线程池（GameInstanceSubsystem）
      • MaxTotalWorkers 默认 4，可通过 URDMSetting 配置（绝对数 / 百分比模式）
```

### 弹孔流水线（客户端 → 服务端 → 所有客户端）

1. **客户端**：`UDestructionProjectileComponent` 命中 → 本地立即显示临时贴花  
2. **客户端 → 服务端**：`UDestructionNetworkComponent::RequestDestruction` → `ServerApplyDestruction_Compact` RPC  
3. **服务端**：安全校验 → `URealtimeDestructibleMeshComponent` 执行布尔运算 → `MulticastApplyOps` 广播  
4. **所有客户端**：接收广播 → 更新网格 + 生成 `ADebrisActor`

---

## 关键约定

### 编辑器工具（`RealtimeDestructionEditor` 模块）

- **Anchor 编辑模式**：`FAnchorEditMode` + `FAnchorEditModeToolkit`，在编辑器中标记 Floor/Anchor Cell
- **弹孔资产编辑器**：`FImpactProfileEditorWindow` + `FImpactProfileEditorViewport`，用于预览贴花效果
- **组件详情面板**：`FRealtimeDestructibleMeshComponentDetails` / `FDestructionProjectileComponentDetails`

### 数据资产（`UImpactProfileDataAsset`）

- 按 `ConfigID`（`FName`）区分不同弹孔配置，在 `URDMSetting` 中注册
- 通过 `URDMSetting::GetImpactProfileDataAsset(ConfigID)` 全局访问
- `SurfaceConfigs`：`TMap<SurfaceType, FImpactProfileConfigArray>`，按物理表面类型查弹孔变体

### 网络压缩（`FCompactDestructionOp`）

- `Radius` 为 `uint8`（1–255 cm），爆炸半径 >255 cm 精度丢失
- `Sequence` 为 `uint16`，65536 次操作后回绕，高频破坏场景需注意乱序风险

### CoreRedirects

`Config/DefaultRealtimeDestruction.ini` 记录了历次重构的 CoreRedirects，修改以下类/属性名后须在此补充重定向：
- `URealtimeDestructibleMeshComponent` 的公开函数名
- `URDMSetting` 的属性名
- GridCell / SuperCell 相关结构体名

---

## 已知问题与约束（修改前须知）

| 问题 | 影响范围 |
|------|----------|
| `URealtimeDestructibleMeshComponent` 头文件超 1200 行，职责过重 | 修改该文件时需小心影响范围 |
| `ADebrisActor` 用 `ProceduralMeshComponent`，主系统用 `DynamicMeshComponent` | 碎片渲染路径与主网格路径不统一 |
| `DebrisSplitCount`、`MinDebrisSyncSize` 等参数标注了 `TODO: Find appropriate values` | 默认值未经严格验证 |
| `GridCellLayout` 依赖编辑器构建时的坐标系，运行时改 `WorldScale` 会错位 | 不得在运行时修改 `WorldScale` |
| 调试可视化（`DrawGridCellDebug` 等）在 `TickComponent` 中每帧执行 | 开启调试模式会有显著性能开销 |
