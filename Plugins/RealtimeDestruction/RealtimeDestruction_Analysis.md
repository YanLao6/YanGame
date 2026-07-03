# RealtimeDestruction 插件源码分析

> 版权：LazyDevelopers（KRAFTON JUNGLE GameTech Lab 参与作品）
> 许可：Fab Standard License

---

## 一、架构总览

插件由两个模块组成：

| 模块 | 用途 |
|------|------|
| `RealtimeDestruction` | 运行时核心逻辑 |
| `RealtimeDestructionEditor` | 编辑器工具（Anchor 编辑模式、Detail 面板、可视化器） |

### 核心系统层次

```
URealtimeDestructibleMeshComponent（主入口，继承 UDynamicMeshComponent）
│
├── 网格层：StaticMesh → GeometryCollection → ChunkMesh（多个 UDynamicMeshComponent）
│
├── 空间索引层：FGridCellLayout（体素网格）
│   ├── SubCell（每个 Cell 细分为 2×2×2=8 个子单元，用于精细破坏判定）
│   └── SuperCell（4×4×4 个 Cell 一组，用于层次化 BFS 加速）
│
├── 布尔运算层：FRealtimeBooleanProcessor
│   ├── 高优先级队列（穿透弹）/ 普通队列（非穿透弹）
│   ├── Per-Chunk 异步 Worker（Union → Subtract 两阶段流水线）
│   └── 自适应简化（防止三角面爆炸增长）
│
├── 结构完整性层：FStructuralIntegritySystem
│   ├── 基于 BFS 的连通性检测（Anchor → 连通 → 孤岛 = 碎片）
│   └── 层次化 BFS（Supercell 快速跳过 + Cell 精细遍历）
│
├── 碎片层：ADebrisActor（ProceduralMeshComponent + BoxComponent 物理）
│
├── 网络层：UDestructionNetworkComponent + UDestructionGameInstanceSubsystem
│
└── 线程管理：URDMThreadManagerSubsystem（全局线程池）
```

---

## 二、关键系统说明

### 2.1 网格分块（Chunk）
- 在编辑器中调用 `GenerateDestructibleChunks()`，利用 GeometryCollection 对 StaticMesh 做 Voronoi 分形
- 运行时每个 Chunk 是独立的 `UDynamicMeshComponent`，布尔操作在各自 Chunk 上异步执行，互不阻塞

### 2.2 体素网格（GridCell / SubCell / SuperCell）
- `FGridCellLayout`：预计算的静态网格，存储每个 Cell 的三角面归属和邻居关系，使用稀疏数组+位域节省内存
- `FSubCell`：1字节（8 bit）表示 8 个子单元存活状态，精度更高的破坏判定
- `FSuperCellState`：4×4×4 Cell 为一组，BFS 时先以 SuperCell 为单位跳过完整区域，显著减少遍历次数

### 2.3 布尔运算处理器（FRealtimeBooleanProcessor）
- **两阶段流水线**：先 Union 多次撞击的工具网格（减少 Subtract 次数），再做 Boolean Subtract
- **自适应调整**：根据每帧 Subtract 耗时动态调整 Union 批次大小和 Simplify 间隔
- **HC 拉普拉斯平滑**（Vollmer 1999）：去除碎片剥离时的阶梯状锯齿感

### 2.4 结构完整性（FStructuralIntegritySystem）
- 纯 C++，线程安全（FRWLock），确定性（相同输入输出相同结果）
- Anchor Cell 标记地板/固定点；破坏后 BFS 找出与 Anchor 断连的孤岛 → 生成 `ADebrisActor`
- 支持 AnchorActor / AnchorPlaneActor / AnchorVolumeActor 三种标记方式

### 2.5 网络同步
- **客户端预测**：客户端本地立即显示临时贴花，Server 处理后广播实际破坏结果
- **压缩传输**：`FCompactDestructionOp` 约 102 bits/请求（原始 ~320 bits），减少约 70% 带宽
- **服务端批量播报**：16ms 窗口收集多客户端请求，一次 Multicast 广播
- **Late Join 同步**：服务端保存全量 Op 历史（最多 10000 条），新客户端加入后补发
- **服务端安全校验**：距离限制、频率限制（每秒最大破坏次数）、视线检测、弹孔半径上限

### 2.6 服务端碰撞（Server Cell Box Collision）
- 专用服务器不做任何 Boolean 网格运算，改用体素化的 BoxSetup 作碰撞盒
- 破坏后只更新对应 Chunk 的碰撞体，不重建完整物理网格，避免服务器帧卡顿

---

## 三、优点

1. **高质量实时破坏视觉**：基于 GeometryScript DynamicMesh 做真实网格布尔操作，破坏孔洞形状由弹道几何决定，效果细腻
2. **结构完整性物理仿真**：BFS 检测悬空碎片，重力坠落物理真实，增强沉浸感
3. **多层次性能优化**：SuperCell → Cell → SubCell 层次 BFS；Per-Chunk 并行异步布尔；自适应 Simplify；Server 端绕过布尔用 Cell 碰撞盒
4. **生产级多人网络**：服务端权威 + 客户端预测 + Late Join 同步 + 安全校验，联机体验完整
5. **网络带宽精打细算**：FCompactDestructionOp 逐字段定长量化，批量播报减少 RPC 头部开销
6. **完善的编辑器工具链**：Anchor 专属编辑模式、弹孔 Profile 资产编辑器、组件 Detail 面板按钮、多类型 Debug 可视化
7. **数据驱动弹孔配置**：ImpactProfileDataAsset 按表面类型配置贴花，无需修改代码
8. **弹孔聚合（Clustering）**：BulletClusterComponent 自动合并密集撞击，增强"打穿墙壁"的玩家反馈感
9. **线程安全设计**：FStructuralIntegritySystem 使用 FRWLock；FRealtimeBooleanProcessor 使用 MPSC 无锁队列；原子计数确保安全关闭

---

## 四、缺点与潜在问题

### 4.1 设计层面

| 问题 | 具体表现 |
|------|----------|
| **主组件职责过重** | `URealtimeDestructibleMeshComponent` 头文件超 1200 行，集成了网络 RPC、碰撞、碎片管理、调试可视化、Late Join、聚合等所有功能，违反单一职责原则 |
| **Debris 同步耦合** | 碎片物理同步仍由 Component 集中管理（代码 TODO 注释承认），应让每个 ADebrisActor 自己处理 Replication |
| **锚点管理分散** | AnchorActor / AnchorPlaneActor / AnchorVolumeActor 三种 Actor 分开摆放，关卡管理复杂，缺少统一的 AnchorManager |
| **无持久化支持** | 当前没有将破坏状态序列化到磁盘的机制，存档/读档场景无法恢复破坏状态 |

### 4.2 性能层面

| 问题 | 具体表现 |
|------|----------|
| **布尔运算仍有峰值风险** | 复杂凸多边形网格做 Subtract 时，GeometryScript 内部耗时不可控，极端情况仍可能造成帧率抖动 |
| **Late Join 历史上限** | `MaxOpHistorySize = 10000`，长时间游戏后新加入玩家只能看到近期破坏，早期状态丢失 |
| **调试可视化在 Tick 里** | DrawGridCellDebug / DrawSupercellDebug 等在 TickComponent 里每帧执行，开启时性能开销不可忽视 |
| **全局线程池默认上限偏小** | `MaxTotalWorkers` 默认 4，多个可破坏物同时被射击时会排队等线程，破坏响应延迟增加 |
| **Debris 物理同步固定 100ms** | 碎片物理状态广播间隔固定（`DebrisPhysicsSyncInterval = 0.1f`），对快速旋转/运动碎片可能出现明显抖动 |

### 4.3 协议与精度层面

| 问题 | 具体表现 |
|------|----------|
| **Compact 半径精度有限** | `FCompactDestructionOp::Radius` 为 `uint8`（1-255 cm），超大破坏半径（如爆炸 >255cm）精度丢失 |
| **GridCell 依赖构建时坐标** | 代码注释明确警告：运行时修改 WorldScale 会造成网格与实际网格不匹配，缺乏自动保护 |
| **序列号回绕风险** | `FCompactDestructionOp::Sequence` 为 `uint16`，65536 次操作后回绕，高频破坏场景可能引发乱序判定 |

### 4.4 代码质量层面

| 问题 | 具体表现 |
|------|----------|
| **未完成的 TODO 参数** | `DebrisSplitCount`、`MinDebrisSyncSize` 等参数注释写明 "TODO: Find appropriate values and remove this"，默认值不确定 |
| **DebrisActor 使用 ProceduralMesh** | 碎片用 ProceduralMeshComponent 渲染，主系统用 DynamicMeshComponent，技术栈不统一，增加维护成本 |
| **命名拼写错误** | `ClusterRaidusOffset`（应为 Radius）、`bShowAffetedChunks`（应为 Affected）等拼写错误散落代码中 |
| **InitializeFromStaticMesh 已废弃** | 对外暴露了 `UE_DEPRECATED(5.7, ...)` 的函数，蓝图用户容易误用 |

---

## 五、总结评估

| 维度 | 评分 | 说明 |
|------|------|------|
| 视觉效果 | ★★★★★ | 真实网格布尔破坏，行业顶级水准 |
| 结构完整性 | ★★★★☆ | BFS 联通检测完善，锚点管理稍繁琐 |
| 多人联机 | ★★★★☆ | 安全校验、带宽压缩、Late Join 齐全 |
| 性能优化 | ★★★★☆ | 多层次优化思路好，峰值控制尚有余地 |
| 代码架构 | ★★★☆☆ | 核心组件职责过重，碎片模块待解耦 |
| 工程完成度 | ★★★☆☆ | 存在多处 TODO/未确定参数，缺持久化 |

**适合场景**：FPS/TPS 多人游戏中墙体、掩体、建筑物等中小型可破坏静态网格，单机或多人均可。
**不适合场景**：需要存档恢复破坏状态的 RPG、需要超大场景大量同时破坏（性能压力）、需要超高精度大半径爆炸破坏。
