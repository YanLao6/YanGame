# 外部牵引飞行

## 能力与范围

`UChaosAttractionMode` 位于 YanGameMover，继承 `UChaosFlyingMode`。外部来源发布球心与中心速度，模式每个物理步将它转换成三维 DirectionalIntent，复用飞行模式的加减速、重力补偿与 Chaos 碰撞执行。

外圈内的角色被持续拉近，进入 `InnerRadius` 后锁定来源。锁定表示持续跟随关系：中心跳变、角色被阻挡或中心移动过快时，角色按速度追赶，允许暂时落后。模式不使用 Blink，也不通过“位置误差 / 物理步长”补齐位置。

被牵引角色的位移由上述模式负责；牵引来源自身的通用视角操控由 `AYanAttractionActor` 负责，蓝球的蓄力、注色、按键映射和销毁规则由 ColorTransfer 负责。

## 牵引来源 Actor

`AYanAttractionActor` 位于 `YanGameMover/Public/Actor/YanAttractionActor.h`，是可供 C++、Blueprint 和 AngelScript 派生的抽象父类。它负责中心位置与速度采样、范围内 Pawn 的收集、牵引快照刷新及来源释放，不持有角色的捕获状态。

- 子类提供场景根组件，并重写 `GetAttractRadius`（C++ 为 `GetAttractRadius_Implementation`）返回当前半径；默认半径为零。范围查询复用引擎 `SphereOverlapActors`，只接受 Pawn 碰撞对象并排除 `Instigator`。
- 在 `Super::BeginPlay()` 之后由服务器调用 `AuthStartAttraction()`，首个刷新在 `AttractionRefreshInterval` 后执行，重复启动不重置计时。父类不会自动启用牵引。
- `AuthStopAttraction()` 停止刷新并释放所有接触过的 Mover；`EndPlay` 自动调用。停止后可再次开启，同一 Actor 的更新序号持续递增，防止旧快照覆盖新状态。
- `InnerRadius`、`PullSpeed` 和 `AttractionRefreshInterval` 由父类持有。目标仍须配置下述牵引模式和 Transition，并支持 `RefreshRadialAttractionToTarget` 的网络物理调度链路。

ColorTransfer 的继承链为 `AYanAttractionActor → AColorChargeOrb → AColorAttractOrb`，保留蓄力技能对 `AColorChargeOrb` 的类型契约。仅蓝球在 `BeginPlay` 开启牵引，通过蓄力进度提供半径，并在广播 `OnControlFinished` 前停止牵引；注色复用父类的范围查询。普通蓄力球不会启动牵引计时器。

## 来源自身的视角操控

父类默认启用 Actor 与移动复制，场景根组件由子类提供。服务器调用 `AuthStartControl()` 后脱离挂接并开始操控；`AuthStopControl()` 只停止来源自身移动，牵引场可继续作用。`MoveSpeed`、`PushSpeed`、`AimDistanceRange` 和操控状态均归父类所有。

- 生成时设置 `Instigator` 为操作者，`Owner` 沿该 Pawn 的拥有连接归属。父类在拥有端调用 C++ 扩展点 `SendLocalControl()`；Blueprint / AngelScript 也可直接将输入交给 `SubmitControlInput()`。蓝球的左右键和左 Ctrl 映射留在 ColorTransfer。
- `SubmitControlInput()` 将意图收敛为 `-1/0/1`，仅变化时调用可靠的 `ServerUpdateControl(int8)`。服务器只接受操控期间的合法值，并按服务器帧时间更新距离；RPC 频率不决定移动距离。控制阶段通过属性复制同步，球的位置通过 Actor 移动复制同步。
- 移动方向复用 `GetOwnerViewLocationAndForward` 读取的 Mover 输入 `ControlRotation`；原点取服务器 Pawn 视点。输入缺失或非法时悬停，客户端不再另传相机位置、朝向或目标点。第三人称相机偏移和相机碰撞不参与操控射线。
- 角色数据来源、采样时刻和反作弊边界见 [服务器角色数据通道](../../.codex/skills/yan-systems/references/mover-input-contract.md#服务器角色数据通道)。共同使用一个数据来源不代表客户端与服务器处于同一帧，也不使客户端视角意图变为可信命中结果。

## 蓝图配置

在角色使用的 Mover **组件蓝图 Class Defaults** 中配置，不要在 Pawn 的组件模板上另建内联模式：

1. 在 `MovementModes` 添加一个名称自定的条目，例如 `Attraction`，实例类型选 `ChaosAttractionMode`。
2. 在全局 `Transitions` 或允许被牵引的模式上添加 `ChaosAttractionEnterCheck`，将 `TransitionToMode` 填为该条目的名称。使用全局判定时按玩法需要安排与其他 Transition 的优先级；已经处于目标模式时不会重入。
3. `ExitMovementMode` 默认 `Falling`。它必须指向另一个已配置的正常移动模式。
4. 飞行速度、加速度、减速度沿用 `USharedChaosCharacterMovementSettings`。球上的 `PullSpeed` 进一步限制外圈拉近和内圈跟随的期望速度。`SlowdownDistance` 控制靠近中心时输入减弱的距离，`StopDistance` 为中心附近的停止距离。

代码不注册 MovementMode，也不写死进入模式的名称。不配置进入判定或名称时，不会自动接管角色。

## 接口与权威状态

`UMoverAuthorityLibrary::RefreshRadialAttractionToTarget(Mover, Settings, bScheduleForSync)` 沿用项目的 Authority Sim Action 调度链路，仅由服务器调用。

- `Settings` 类型为 `FYanAttractionSettings`；`SourceId` 非零且由服务器确定，同一来源的 `UpdateSequence` 递增。
- `Location`、`CenterVelocity` 是外部来源快照；接口填写目标 Mover 的 `SnapshotSimTimeMs`。`Radius <= 0` 释放指定来源。
- `DurationMs` 是未捕获时的刷新租期，也限制旧速度的外推窗口。锁定后刷新中断不会丢失捕获关系，但中心停止沿旧速度无限外推。
- `FYanAttractionState` 保存来源、租期、当前来源和捕获状态，参与序列化、纠正和回滚。角色持续选择有效的当前来源；没有当前来源时按 `SourceId` 确定性选择，多个来源不会逐刷新争抢。
- `UChaosAttractionMode` 在局部输入副本中替换 DirectionalIntent；原角色客户端上传的输入包保持可重演。物理线程不访问球 Actor、相机或游戏线程缓存。

`FYanRadialAttractionUpdateEffect` 只写控制状态，不修改角色位置、速度或 LayeredMove。普通的 `ApplyRadialAttractionToTarget` 与 `FYanLayeredMove_RadialAttraction` 继续承担可叠加的径向拉扯；捕获跟随只有 Mode 这一处实现。

## 属性复制与模拟动作的边界

`FYanAttractionSettings` 已通过 Authority Sim Action 同步到目标的网络物理模拟；接收动作进入 `UChaosMoverSimulation::ApplyAction_Internal`，再由可回滚的 Instant Movement Effect 写入 `FYanAttractionState`。每个目标的 `SnapshotSimTimeMs` 由现有接口填写，捕获状态仍由该目标的 SyncState 持有。

把来源上的 `Settings` 直接标记为普通 `Replicated` 属性不能等价替代这条链路：`OnRep` 的游戏线程到达时间不提供统一的生效物理帧，也不自动记录动作历史供重模拟使用。模拟线程不得直接读取来源 Actor 的实时属性。即使保留 `OnRep` 桥接，也仍需明确生效帧、历史、迟到处理和来源释放，而不能让各端在收到属性的当帧自行切换模拟状态。

若目的是减少高频可靠消息，应先评估现有 NetworkPhysics 动作传输：`EnqueueScheduledAction_External` 的 `bReliable=false` 使用框架的不可靠属性复制路径，仍属于带物理帧的 Sim Action。当前牵引接口明确使用 `bReliable=true`；可研究将可覆盖的中心快照与必须送达的释放动作分开处理，但需要验证丢包下的租期、乱序、捕获和释放行为，不能仅凭编译或属性复制名称判断更优。仅供表现的参数可以按需普通复制，不另建捕获状态的权威副本。

## 验收与限制

- `YanGame.Mover.RadialAttraction` 自动化测试覆盖状态刷新不改位置/速度、回滚历史隔离、外圈有效性、内圈锁定、来源释放、蓝图命名、序列化与三维飞行输入。
- 速度回归使用 5/10/25 ms 物理步、初速为零、飞行上限 800 cm/s、牵引上限 600 cm/s、加速度 4000 cm/s²、减速度 8000 cm/s²，调用引擎的自由飞行速度计算。该测试不替代 Chaos 场景碰撞或联机测试。
- 角色蓝图配置完成后，需在双客户端验证外圈拉近、内圈锁定、快速转动球心、墙体阻挡、球销毁恢复和多来源选择；加入延迟/丢包后观察运动连续性及纠正情况。
- 外部快照仍有网络和调度延迟，新增模式不能消除这些延迟。已有高速惯性由飞行减速度处理，`PullSpeed` 限制期望速度，不强制清零入场惯性。
