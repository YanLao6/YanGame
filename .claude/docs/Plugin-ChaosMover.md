# CLAUDE.md — ChaosMover 插件(版本：5.8.0)

> 在 Chaos 物理线程上、基于 **Mover 框架** 实现物理驱动的角色/物体移动。

---

## 1. 定位与依赖

| 项 | 值 |
|---|---|
| 模块 | `ChaosMover`（Runtime，`LoadingPhase = Default`） |
| 成熟度 | `IsExperimentalVersion = true`，接口随版本变动，勿视为稳定契约 |
| 硬依赖插件 | **Mover**（本插件是 Mover 的一个物理后端实现） |
| Public 依赖模块 | `Core` `GameplayTags` `InputCore` `NetCore` `Mover` `Water` |
| Private 依赖模块 | `CoreUObject` `DeveloperSettings` `Engine` `MotionWarping` |
| 物理支持 | `SetupModulePhysicsSupport(Target)`（Chaos） |

**前置开关（必需）**：`Config/DefaultChaosMover.ini` 强制开启
`[/Script/Engine.PhysicsSettings] PhysicsPrediction=(bEnablePhysicsPrediction=True)`。
ChaosMover 依赖 **网络物理预测(Network Physics Prediction)** 与固定步长异步物理，缺少此开关无法正常联网预测。

---

## 2. 分层架构（最重要）

ChaosMover 把「移动仿真」从游戏线程搬到 Chaos 物理线程。数据流分四层：

```
游戏线程 (GT)                           物理线程 (PT，固定 dt，异步)
┌───────────────────────────┐          ┌────────────────────────────────┐
│ UChaosCharacterMoverComponent │  输入  │ UChaosMoverSimulation             │
│  (: UCharacterMoverComponent) │ ─────► │  (: UMoverSimulation)            │
│  · ProduceInput / 事件派发     │        │  · ProcessInputs / SimulationTick │
│  · OnLanded / OnJumped 委托    │ ◄───── │  · FMoverStateMachine 驱动模式    │
└──────────────┬────────────────┘  输出  │  · 操作 Chaos 约束/粒子           │
               │                         └────────────────────────────────┘
        BackendClass                                  ▲
               ▼                                       │ 每物理 tick 后驱动
┌───────────────────────────┐          ┌────────────────────────────────┐
│ UChaosMoverBackendComponent │ 创建/桥接 │ UChaosMoverSubsystem              │
│  (: IMoverBackendLiaisonInterface)     │  (: UWorldSubsystem)             │
│  · 创建物理(粒子+约束)/仿真对象 │        │  · Register/Unregister 后端       │
│  · Produce/ConsumeData        │        │  · OnPostPhysicsTick → 异步回调    │
│  · Rollback / 事件调度         │        │  · FAsyncCallback (物理 solver)   │
└───────────────────────────┘          └────────────────────────────────┘
```

- **GT 组件**（`UChaosCharacterMoverComponent`）：玩家可见的一端。产生输入、暴露蓝图委托
  （`OnLandedDelegate`/`OnJumped`）、发起 `Launch`、设置轨迹预测参数。它把 `BackendClass`
  默认设为 `UChaosMoverBackendComponent`。
- **后端**（`UChaosMoverBackendComponent`，`Within = MoverComponent`）：实现 `IMoverBackendLiaisonInterface`。
  负责创建受控物理对象、**Character Ground Constraint**（角色贴地约束）与 **Actuation Joint Constraint**
  （通用驱动关节），把 GT 的输入注入仿真，把仿真输出取回，并处理 rollback/resim。
- **子系统**（`UChaosMoverSubsystem`）：World 级单例，收集所有后端，在物理场景 tick 后统一驱动异步回调。
- **仿真**（`UChaosMoverSimulation`）：真正跑移动逻辑的地方，运行在物理线程，内含 `FMoverStateMachine`
  管理移动模式切换。同时实现 `INetworkPhysicsActionHandler_Internal` 以接收网络化的"sim action"。

> **线程约束红线**：凡带 `_Async` 后缀、`SimulationTick_Implementation`、`GenerateMove_Implementation`、
> `ExecuteMove_Async`、`PreSimulationTick_Async`/`PostSimulationTick_Async` 的代码，**一律禁止访问
> `AActor`/`UActorComponent` 及任何 GT-only 对象**。需要网格偏移等 GT 数据时，在 `OnModeRegistered`/
> `OnRegistered`（GT）阶段缓存，或经 `FChaosMoverSimulationDefaultInputs`（如
> `PrimaryVisualComponentRelativeTransform`）传入。

---

## 3. 目录导航

```
Source/ChaosMover/
├─ Public/ChaosMover/
│  ├─ ChaosMoverSimulation.h            物理线程仿真主体（核心）
│  ├─ ChaosMoverSimulationTypes.h       输入/输出/黑板数据、接口、事件、网络队列结构（核心类型库）
│  ├─ ChaosMovementMode.h               所有 Chaos 移动模式基类 UChaosMovementMode
│  ├─ ChaosCompositeMovementMode.h      组合模式 = MoveSource + MoveExecutor
│  ├─ ChaosMoveExecutorBase.h           移动执行器基类（"如何把 move 施加到物理"）
│  ├─ ChaosMoverSourceBase.h            移动来源基类（"产生什么速度"）
│  ├─ ChaosMovementModeTransition.h     模式转换基类
│  ├─ ChaosMoverStateMachine.h          物理线程状态机 FMoverStateMachine
│  ├─ ChaosMoverActionTypes.h           网络物理 action 载荷类型
│  ├─ ChaosMoverBlueprintLibrary.h      蓝图静态库（服务器权威 move/effect 入队）
│  ├─ ChaosMoverConsoleVariables.h      CVar 声明
│  ├─ Backends/ChaosMoverBackend.h      后端联络组件
│  ├─ Character/
│  │  ├─ ChaosCharacterMoverComponent.h GT 角色移动组件（用户主入口）
│  │  ├─ ChaosCharacterInputs.h         输入数据块（Launch/Crouch/设置覆盖/RequestedMove）
│  │  ├─ Modes/                         Walking/SimpleWalking/SmoothWalking/Falling/Flying/Swimming
│  │  ├─ Executors/                     ChaosCharacterWalkingExecutor / ChaosCharacterAirExecutor
│  │  ├─ Effects/                       ChaosCharacterApplyVelocityEffect（Launch 用）
│  │  ├─ Modifiers/                     ChaosStanceModifier（站姿/蹲伏）
│  │  ├─ Transitions/                   Crouch/Falling/Jump/Landing/Launch/Water 检查
│  │  └─ Settings/                      SharedChaosCharacterMovementSettings（跨模式共享设置）
│  └─ PathedMovement/                   平台/物体沿路径移动子系统
│     ├─ ChaosPathedMovementMode.h              路径移动模式基类
│     ├─ ChaosPathedMovementControllerComponent.h 路径播放控制器
│     ├─ ChaosPathedMovementPatternBase.h       路径图案基类
│     ├─ ChaosPoint/Spline/EllipticalMovementPathPattern.h  具体路径图案
│     └─ Transitions/ChaosPathedMovementReachedEndTransition.h
└─ Private/                             上述各项实现 + Backends/(Subsystem/AsyncCallback/NetworkData)
                                        + Utilities/(ChaosGroundMovementUtils / ChaosMoverQueryUtils)
```

---

## 4. 核心类速查

| 类 | 线程 | 职责 |
|---|---|---|
| `UChaosCharacterMoverComponent` | GT | 用户主入口：加到 Pawn 上即获得物理角色移动 |
| `UChaosMoverBackendComponent` | GT | 创建物理与仿真，桥接 GT↔PT，处理 rollback |
| `UChaosMoverSubsystem` | GT | 收集后端，物理 tick 后统一驱动 |
| `UChaosMoverSimulation` | PT | 移动仿真主体，持有状态机与物理约束句柄 |
| `FMoverStateMachine`（`UE::ChaosMover`） | PT | 管理当前/排队移动模式、转换、修饰符 |
| `UChaosMovementMode` | PT | 所有 Chaos 移动模式基类（Abstract, Blueprintable） |
| `UChaosCharacterMovementMode` | PT | 角色移动模式基类，接入 Character Ground Constraint |
| `UChaosCompositeMovementMode` | PT | 组合 `MoveSource` + `MoveExecutor` 的可配置模式 |
| `USharedChaosCharacterMovementSettings` | 数据 | 跨模式共享的加减速/摩擦/坡度/步高等设置 |

---

## 5. 角色移动模式清单（`Character/Modes/`）

默认在 `UChaosCharacterMoverComponent` 构造时注册三种，`StartingMovementMode = Falling`：

| 模式名（`DefaultModeNames`） | 类 | 说明 |
|---|---|---|
| `Walking` | `UChaosWalkingMode` | 贴地行走，驱动 Character Ground Constraint。关键参数：`GroundDamping` `FrictionForceLimit` `FractionalRadialForceLimitScaling` `FractionalGroundReaction` `MaxStepHeight`（来自共享设置） |
| `Falling` | `UChaosFallingMode` | 空中/下落。空气控制 `AirControlPercentage`、终端速度 `TerminalVerticalSpeed`、落地清零 `bCancelVerticalSpeedOnLanding` 等 |
| `Flying` | `UChaosFlyingMode` | 自由飞行 |
| （可选）`Swimming` | `UChaosSwimmingMode` | 配合 Water 插件的游泳，`ChaosCharacterWaterCheck` 触发进入 |
| （可选） | `UChaosSimpleWalkingMode` | 简化行走模式，SmoothWalking 的父类 |
| （可选） | `UChaosSmoothWalkingMode` | 移植 Mover `USmoothWalkingMode` 的弹簧/平滑模型；用 `FChaosSmoothWalkingState` 持久化弹簧中间量。参数在 `Smooth Walking Settings` 分类下 |

> 提示：GT 组件默认把 `bHandleJump = false`、`bHandleStanceChanges = false`，跳跃/蹲伏改由
> **物理线程的转换(Transition)与修饰符(Modifier)** 处理（见下）。

---

## 6. 转换 / 修饰符 / 效果 / 输入

### 转换 Transitions（`Character/Transitions/`，继承 `UChaosMovementModeTransition`）
每帧在物理线程 `Evaluate_Implementation` 判定，命中则 `Trigger_Implementation` 切模式。

| 转换 | 触发条件 → 目标 |
|---|---|
| `UChaosCharacterJumpCheck` | 跳跃输入 → `TransitionToMode`；参数 `JumpUpwardsSpeed`、`FractionalGroundReactionImpulse` |
| `UChaosCharacterFallingCheck` | 离地/超过相对速度阈值 → Falling |
| `UChaosCharacterLandingCheck` | 命中可站立地面 → Walking |
| `UChaosCharacterLaunchCheck` | 收到 Launch 输入 → 对应模式（配合 `Launch()`） |
| `UChaosCharacterCrouchCheck` | 蹲伏输入 → 应用/移除 Stance 修饰符 |
| `UChaosCharacterWaterCheck` | 进入水体 → Swimming |

### 修饰符 Modifier（`Character/Modifiers/`）
- `UChaosStanceModifier`：站姿/蹲伏。改变碰撞体高度并派发 `FStanceModifiedEventData`（`bReEmitOnResim=true`）。
  GT 端 `UChaosCharacterMoverComponent` 用 `PendingBatchFinalStance` 批量归并，避免 resim 抖动导致虚假 `OnStanceChanged`。

### 效果 Effect（`Character/Effects/`）
- `UChaosCharacterApplyVelocityEffect` / `EChaosMoverVelocityEffectMode`（`Impulse` / `AdditiveVelocity` / `OverrideVelocity`）：
  `Launch()` 底层用它给粒子施加冲量或速度。

### 输入数据块（`Character/ChaosCharacterInputs.h`，均 `FMoverDataStructBase`）
按需塞进输入集合，物理线程消费：
- `FChaosMoverLaunchInputs`：发射（速度/冲量 + Mode）
- `FChaosMoverCrouchInputs`：`bWantsToCrouch`
- `FChaosMovementSettingsOverrides` / `...Remover`：运行时覆盖某模式的 `MaxSpeed`/`Acceleration`
- `FChaosMoverRequestedMoveInputs`：请求速度（`RequestedVelocity` + `bForceMaxSpeed` + `bUseAcceleration`）

---

## 7. 组合模式：Source + Executor（推荐的扩展方式）

`UChaosCompositeMovementMode` 把「产生什么速度」和「如何施加到物理」解耦：

- **`MoveSource : UChaosMoverSourceBase`** — 每 tick 产出 `FProposedMove`；`IsFinished()` 返回 true 时自动切到 `NextModeName`。
- **`MoveExecutor : UChaosMoveExecutorBase`** — `ExecuteMove_Async` 把 `FProposedMove` 施加到仿真输出（计算目标位移、更新 sync state、处理表面交互）。

若 Executor 实现了 `IChaosCharacterMovementModeInterface` 和/或 `IChaosCharacterConstraintMovementModeInterface`，
`CollectSimulationInterfaces` 会自动把它们暴露给仿真——**无需子类化**。内置 Executor：
`UChaosCharacterWalkingExecutor`（贴地约束）与 `UChaosCharacterAirExecutor`（空中）。

**Pre/Post 仿真回调接口**（`ChaosMoverSimulationTypes.h`）：给任意 Mode/Executor 附加
`IChaosPreSimulationTickInterface` / `IChaosPostSimulationTickInterface` 即可参与每 tick 的
`PreSimulationTick_Async` / `PostSimulationTick_Async` 分发（做地面扫描、把粒子速度写回等）。
Post 回调若不自行施加粒子速度，可在 `FChaosMoverPostSimContext` 里置 `bApplyFallbackVelocity = true` 让仿真兜底。

---

## 8. 路径移动（`PathedMovement/`，用于平台/机关/移动物）

让**物体**（非角色）沿预定义路径运动，计算在物理线程完成，能正确与其它刚体交互。

- `UChaosPathedMovementControllerComponent`：路径播放控制（时长、倒放、循环、时间膨胀）。
- `UChaosPathedMovementMode`：路径模式基类。
  - `bUseJointConstraint = true`：用关节把物体"拉"向路径点，可对物理力有柔性响应（晃动/停顿）。
  - `bUseJointConstraint = false`：运动学刚性移动，无视物理力，绝不偏离路径。
  - `PathPatterns`（**有序**，逐帧从前往后累积求值）+ `Easing`/`CustomEasingCurve` + `OneWayPlaybackDuration`。
  - 运行前用 `SetOneWayTripDuration_BeginPlayOnly` 设时长；运行中改用控制器的 `SetMovementTimeDilation`。
- 路径图案：`UChaosPointMovementPathPattern`（点）、`UChaosSplineMovementPathPattern`（样条）、
  `UChaosEllipticalMovementPathPattern`（椭圆）。
- `UChaosPathedMovementReachedEndTransition`：到达终点触发转换。
- `UChaosPathedMovementDebugDrawComponent`：调试绘制聚合路径。

---

## 9. 网络与预测（联网必读）

ChaosMover 走 **Chaos 网络物理预测 + resim**。发起 LayeredMove / InstantMovementEffect 有两条网络路径，由 CVar 切换：

| 路径 | CVar（默认 `true`） | 检测时机 | 特性 |
|---|---|---|---|
| **Sim Actions（Proposed）** | `ChaosMover.Networking.NetworkMovesWithSimActions` / `...NetworkInstantMovementEffectsWithSimActions` | **PostSim** 读 `GetLastInputCmd()` | 服务器权威、抗作弊；服务器相对 PreSim 有 1 帧延迟 |
| **内嵌 Input Cmd** | 同上置 `false` | **PreSim** 读输入 | 客户端权威、适合合作(co-op) |

> **蓝图关键陷阱**：当以 sim actions 方式联网时，蓝图必须在 **PostSim** 通过 `GetLastInputCmd()` 检测条件，
> **不能**读 PreSim 输入。可用 `UChaosMoverBlueprintLibrary::IsNetworkingMovesWithSimActions()` /
> `IsNetworkingInstantMovementEffectsWithSimActions()` 在运行时判断当前路径。

**服务器权威入队 API**（`UChaosMoverBlueprintLibrary`，Server-only，需要 ChaosMover 后端；无权限时静默 no-op）：
- `Queue Layered Move (Authority)` / `Queue Instant Movement Effect (Authority)`：立即入队、复制到客户端（无客户端预测）。
- `Schedule ... (Authority)`：延迟到未来某帧，让所有端在**同一物理帧**应用，避免客户端纠正（适合强制击飞、传送）。
  延迟取自 `UNetworkPhysicsSettingsComponent::EventSchedulingMinDelaySeconds`。

**容差**：想让客户端在容差内偏离服务器而不触发 resim，重写 `FInstantMovementEffect` / `FLayeredMoveBase`
子类型的 `IsNearlyEqualTo`。

**Rollback 语义**：网络收到的 move/effect **总是回滚**；仅 GT 本地发起的（`bShouldRollBack=false`）不回滚，
因为 resim 不会重跑 GT 逻辑、无法重新生成它们。

---

## 10. 常用 API 与蓝图入口

**GT 组件 `UChaosCharacterMoverComponent`**：
- `Launch(VelocityOrImpulse, Mode)` — 需当前模式实现了 Launch 转换才生效。
- `OverrideMovementSettings(FChaosMovementSettingsOverrides)` / `CancelMovementSettingsOverrides(ModeName)`。
- `TryGetFloorCheckHitResult()` / `TryGetLastWaterResult()`。
- 轨迹预测：`GetPredictedTrajectory(FMoverPredictTrajectoryParams)`、`SetPredictedTrajectoryParams(Steps, StepSeconds, bEnable)`。
- 委托：`OnLandedDelegate`、`OnJumped`。

**仿真 `UChaosMoverSimulation`（PT，多为 BlueprintCallable/Pure）**：
- 排队：`Queue Layered Move` / `Queue Instant Movement Effect` / `Queue Movement Modifier`（K2 CustomThunk）。
- 查询：`FindMovementModeByName`、`HasMovementMode`、`HasGameplayTag`、`FindMovementModifierByType<T>()`。
- 传送：`AttemptTeleport`（先 `CanTeleport` 校验再执行，并广播成功/失败事件）。
- 本地非网络输入：`GetLocalSimInput()` / `GetLocalSimInput_Mutable()`。

---

## 11. 扩展指南（怎么做常见需求）

- **新角色移动模式**：优先用 `UChaosCompositeMovementMode` 配置 `MoveSource` + `MoveExecutor`；
  需要贴地约束时让 Executor 实现 `IChaosCharacterConstraintMovementModeInterface`（提供 `GetTargetHeight`
  / `ShouldEnableConstraint` / `UpdateConstraintSettings`）。仅当组合无法满足时再子类化 `UChaosCharacterMovementMode`。
- **每帧物理钩子**：给 Mode/Executor 附加 `IChaosPreSimulationTickInterface` / `IChaosPostSimulationTickInterface`，
  在 `CollectSimulationInterfaces` 里被自动发现，无需改动仿真主体。
- **新转换条件**：继承 `UChaosMovementModeTransition`，实现 `Evaluate_Implementation`（判定）+ `Trigger_Implementation`（执行）。
- **需要 GT 数据**：在 `OnModeRegistered`/`OnRegistered`（GT 阶段）缓存，切勿在 `_Async` 里现取。
- **自定义碰撞忽略**：模式基类的 `IgnoredCollisionMode`（`EnableCollisionsWithIgnored` / `DisableCollisionsWithIgnored`）。

---

## 12. 控制台变量与调试

**CVar（前缀 `ChaosMover.`，见 `ChaosMoverConsoleVariables.cpp`）**：

| CVar | 默认 | 用途 |
|---|---|---|
| `ForceSingleThreadedGT` / `ForceSingleThreadedPT` | `true` | 强制 GT/PT 单线程（便于调试） |
| `DebugDraw.GroundQueries` / `DebugDraw.OverlapQueries` | `false` | 绘制地面/重叠查询 |
| `Perf.SkipGenerateMoveIfOverridden` | `true` | 分层移动完全覆盖时跳过 `GenerateMove` 提速 |
| `FallingCheckRelativeSpeedLimit` | `60` | 超过则判定角色脱离表面 |
| `Networking.NetworkMovesWithSimActions` | `true` | LayeredMove 网络路径（见第 9 节） |
| `Networking.NetworkInstantMovementEffectsWithSimActions` | `true` | InstantEffect 网络路径 |
| `Networking.DisableResimDuplicateEventChecking` | `true` | resim 允许重发相同事件 |
| `Networking.InstantMovementEffectIDHistorySize` / `LayeredMoveIDHistorySize` | `30` | 去重历史帧数 |
| `Networking.FrameDifferenceLeniencyForEventTimeComparison` | `1` | 事件同帧判定容差 |
| `EnablePreSimGroundCheck` / `EnableServerLaunchOverride` | `false` | 临时开关 |

**调试控制台命令**：
- `ChaosMover.Debug.TeleportClientOnly` — 仅本地把角色上抬 500，验证本地-only 瞬时效果。
- `ChaosMover.Debug.TeleportTo <X> <Y> <Z> <Yaw> <Pitch> <Roll>` — 传送本地角色；用 `=` 保持某分量不变
  （如 `... 0 0 = 0` 保持 Z 与 pitch/roll 不变）。
- **Chaos Visual Debugger (CVD)**：仿真会 `TraceMoverData`，配合 `GetDebugSimData()` 输出
  `FChaosMoverTimeStepDebugData` / `FNetworkPhysicsDebugData` / 事件调试数据到 CVD。

---

## 13. 约束红线（改动前对照）

1. **线程**：`_Async` / `SimulationTick` / `GenerateMove` / `ExecuteMove_Async` / Pre-Post 钩子内**禁止**触碰
   AActor/UActorComponent 或任何 GT-only 对象。
2. **确定性**：物理线程逻辑必须可 resim 复现，不得依赖随机、帧率或 GT 状态；网络 move/effect 默认回滚，
   仅本地发起的置 `bShouldRollBack=false`。
3. **前置开关**：确保项目启用 `bEnablePhysicsPrediction`，否则联网预测失效。
4. **实验性**：本插件接口可能随引擎版本变动，勿把其内部实现细节固化为上层项目的硬约束。
5. **数据传递**：跨线程/网络的数据一律经 `FMoverDataStructBase` 子类（实现 `Clone`/`NetSerialize`/`ShouldReconcile`/
   `Interpolate`/`Merge`），不要走裸指针或 GT 引用。
