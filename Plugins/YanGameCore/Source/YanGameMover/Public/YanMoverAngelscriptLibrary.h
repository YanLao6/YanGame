#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MovementModifier.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "YanMoverDataModelTypes.h"
#include "MoverMode/Fatigue/YanFatigueState.h"
#include "MoverLayeredMove/YanRadialAttractionMove.h"

#include "YanMoverAngelscriptLibrary.generated.h"

#define UE_API YANGAMEMOVER_API

class APawn;
class AYanHookProjectile;
class UAbilitySystemComponent;
class UGameplayEffect;
class UMoverBlackboard;

/**
 * 供 AngelScript 调用的 Mover 封装（引擎部分类型/方法未对脚本暴露时由此转发）。
 */
UCLASS(MinimalAPI)
class UYanMoverAngelscriptLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 从 FMoverDataCollection 读取 FMoverDefaultSyncState（存在则 true 并写入 Out）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Data")
	static UE_API bool GetDefaultSyncState(const FMoverDataCollection& Collection, FMoverDefaultSyncState& OutSyncState);

	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Data")
	static UE_API bool GetDefaultInputs(const FMoverDataCollection& Collection, FCharacterDefaultInputs& OutInputs);

	/** 从输入命令包读取项目自定义输入（存在则 true 并写入 Out）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Data")
	static UE_API bool GetYanCharacterInputs(const FMoverDataCollection& Collection, FYanCharacterInputs& OutInputs);

	/** 等价于 FMoverTickEndData::InitForNewFrame（清空 MovementEndState 与 MoveRecord）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|TickEnd")
	static UE_API void InitTickEndForNewFrame(UPARAM(ref) FMoverTickEndData& TickEndData);


	// ---- FMovementModeTickEndState ----

	/** 调用 ResetToDefaults，清空 RemainingMs / NextModeName 等。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementModeTickEnd")
	static UE_API void ResetMovementModeTickEndState(UPARAM(ref) FMovementModeTickEndState& EndState);

	// ---- FMovementRecord / FMovementSubstep ----

	/** 清空 MovementRecord。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static UE_API void ResetMovementRecord(UPARAM(ref) FMovementRecord& Record);

	/** 构造一条 FMovementSubstep（成员在引擎侧未必对脚本暴露字段）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static UE_API FMovementSubstep MakeMovementSubstep(FName MoveName, FVector MoveDelta, bool bIsRelevant);

	/** 追加一条 substep。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static UE_API void AppendMovementSubstep(UPARAM(ref) FMovementRecord& Record, FMovementSubstep Substep);

	/** 总位移 Delta 累计。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static UE_API FVector GetMovementRecordTotalMoveDelta(const FMovementRecord& Record);

	/** 计入速度/状态的「相关」位移部分。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static UE_API FVector GetMovementRecordRelevantMoveDelta(const FMovementRecord& Record);

	/** 由相关位移与 DeltaSeconds 推导的速度。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static UE_API FVector GetMovementRecordRelevantVelocity(const FMovementRecord& Record);

	/** 拷贝一份 substep 列表（用于调试或遍历）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static UE_API TArray<FMovementSubstep> GetMovementRecordSubsteps(const FMovementRecord& Record);

	/** 锁定 relevancy 判定结果。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static UE_API void LockMovementRecordRelevancy(UPARAM(ref) FMovementRecord& Record, bool bLockValue);

	/** 解除 relevancy 锁定。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static UE_API void UnlockMovementRecordRelevancy(UPARAM(ref) FMovementRecord& Record);

	/** 设置本记录对应的总 Delta 时间（秒）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static UE_API void SetMovementRecordDeltaSeconds(UPARAM(ref) FMovementRecord& Record, float DeltaSeconds);

	/** 调试字符串。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static UE_API FString MovementRecordToString(const FMovementRecord& Record);

	/** 获取 SyncState 世界空间线速度（自动处理 MovementBase 坐标变换）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|SyncState")
	static UE_API FVector GetSyncStateVelocity_WorldSpace(const FMoverDefaultSyncState& SyncState);

	/** 获取 SyncState 世界空间朝向（自动处理 MovementBase 坐标变换）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|SyncState")
	static UE_API FRotator GetSyncStateOrientation_WorldSpace(const FMoverDefaultSyncState& SyncState);

	/** 从 MoverComponent 已注册的 Logic 列表中找到指定类型的实例（不存在返回 nullptr）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|LayeredMove")
	static UE_API ULayeredMoveLogic* FindRegisteredMoveLogic(UMoverComponent* MoverComp, TSubclassOf<ULayeredMoveLogic> MoveLogicClass);

	/**
	 * 从 MoverBlackboard 读取上一帧地面法线（世界空间 ImpactNormal）。
	 * 无地面数据或地面不可行走时返回 MoverComp 的 UpDirection。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Floor")
	static UE_API FVector GetLastFloorNormal(UMoverComponent* MoverComp);

	/**
	 * 读取 Mover 最近一次同步状态里某疲劳通道的记录，供 UI 显示冷却。
	 * 通道不存在即已完全恢复，返回 false；本地受控端读到的是预测值。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Fatigue")
	static UE_API bool GetFatigueEntry(const UMoverComponent* MoverComp, EYanFatigueChannel Channel, FYanFatigueEntry& OutEntry);

	/** 黑板中是否存在有效的可行走地面记录（IsWalkableFloor() == true）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Floor")
	static UE_API bool HasWalkableFloor(UMoverComponent* MoverComp);

	/** LayeredMoveLogic 变体：直接接受 SimBlackboard，内部通过 Outer 推导 MoverComponent。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Floor")
	static UE_API bool HasWalkableFloorFromBlackboard(UMoverBlackboard* SimBlackboard);

	/** 从 FCharacterDefaultInputs 获取世界空间移动输入方向（自动处理 MovementBase 坐标变换）。
	 *  AS 无法直接调用非 UFUNCTION 的 GetMoveInput_WorldSpace，通过此函数桥接。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Data")
	static UE_API FVector GetMoveInputWorldSpace(const FCharacterDefaultInputs& Inputs);

	/** 向 MoverComponent 请求一次瞬时速度效果（Chaos 专用 FChaosCharacterApplyVelocityEffect）。
	 *  AS 侧无法直接传 CustomThunk 通配结构体，通过此函数桥接。
	 *  用 Chaos 专用效果是因为通用 FApplyVelocityEffect 仅有同步实现、在 ChaosMover 异步仿真下不生效。
	 *  @param bAdditive true 则叠加到当前速度（AdditiveVelocity），false 则覆盖（OverrideVelocity）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Dash")
	static UE_API void QueueChaosCharacterApplyVelocityEffect(UMoverComponent* MoverComp, FVector WorldSpaceVelocity, bool bAdditive);

	/** 排队一个按比例缩放最大速度的 Modifier，返回可用于取消的句柄。
	 *  蓝图与 AS 侧都无法直接传 CustomThunk 通配结构体，通过此函数桥接。
	 *  瞄准、负重一类持续减速须走 Modifier：直接改移动模式上的速度既不复制也会被回滚覆盖。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Modifier")
	static UE_API FMovementModifierHandle QueueMaxSpeedScale(UMoverComponent* MoverComp, float SpeedScale);

	/** 强制切换 MovementMode（零速度叠加 + ForceMovementMode，保持当前速度不变）。
	 *  钩锁激活时用于确保玩家进入 Falling 模式。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Grappling")
	static UE_API void QueueApplyVelocityEffect(UMoverComponent* MoverComp, FName NewModeName);

	/** 从 UMoverBlackboard 的 Outer 推导重力加速度向量。
	 *  ULayeredMoveLogic::GenerateMove 无直接 MoverComp 参数，通过此函数桥接。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Grappling")
	static UE_API FVector GetGravityFromBlackboard(UMoverBlackboard* SimBlackboard);

	/**
	 * 获取 Pawn 拥有者的视点世界坐标与前向量，用于发射/瞄准取位。
	 * 原点取 Pawn::GetPawnViewLocation()，方向取上传服务器的输入 ControlRotation：
	 * 两者在客户端与服务器确定性一致，不依赖纯客户端的 PlayerCameraManager。
	 * 失败时（拥有者非 Pawn）返回 false，Out 参数不写入。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Grappling")
	static UE_API bool GetOwnerViewLocationAndForward(UMoverComponent* MoverComp, FVector& OutLocation, FVector& OutForward);

	/**
	 * 在指定世界位置和朝向 Spawn AYanHookProjectile，由 MoverComp 提供世界上下文。
	 * 失败时（MoverComp 无效、ProjectileClass 为空、World 无效）返回 nullptr。
	 * 使用 AlwaysSpawn 策略，Owner 设为 MoverComp 的拥有者。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Grappling")
	static UE_API AYanHookProjectile* SpawnHookProjectile(UMoverComponent*                MoverComp,
	                                               TSubclassOf<AYanHookProjectile> ProjectileClass,
	                                               FVector                         SpawnLocation,
	                                               FRotator                        SpawnRotation);

	/**
	 * 从摄像机位置沿前向量做钩锁 LineTrace（WorldStatic 通道），返回是否命中及命中点。
	 * 自动忽略 Pawn 自身。MaxDistance 为本次探测最大距离（cm）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Grappling")
	static UE_API bool LineTraceForGrapple(UMoverComponent* MoverComp, float MaxDistance, FVector& OutImpactPoint);

	/**
	 * 沿给定方向扫描瞬移终点，以角色自身胶囊为形状做 sweep，被挡则停在障碍前。
	 *
	 * 终点必须由发起端一次算定并随输入上传：扫描依赖 game-thread 的碰撞场景，
	 * 客户端与服务器各自扫描不保证得到同一结果，两端各算一次会造成轨迹分歧。
	 * 不校验落点是否可站立——瞬移到空中由后续的下落模式接管，属预期行为。
	 *
	 * @param Direction         瞬移方向（世界空间，内部归一化）
	 * @param MaxDistance       最大瞬移距离（cm）
	 * @param OutTargetLocation 扫描所得终点（世界空间，胶囊中心）
	 * @return 方向或距离无效时返回 false，Out 参数不写入
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Blink")
	static UE_API bool SweepBlinkTarget(UMoverComponent* MoverComp, FVector Direction, float MaxDistance, FVector& OutTargetLocation);

	/**
	 * 向目标 MoverComponent 施加 FLayeredMove_Launch 击飞效果。
	 * AS 无法直接构造 TSharedPtr<FLayeredMoveBase>，通过此函数桥接。
	 * @param LaunchVelocity    击飞速度向量（世界空间，cm/s）
	 * @param ForceMovementMode 发射前强制切换的移动模式（NAME_None 则不切换）
	 * @param DurationMs        持续时间（毫秒），0 = 单帧冲击
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Launch")
	static UE_API void QueueLaunchMove(UMoverComponent* MoverComp, FVector LaunchVelocity, FName ForceMovementMode, float DurationMs);

	/**
	 * 以服务器权威方式向目标 Mover 施加瞬时速度，用于跨 Pawn 施力（击飞、念力推拉等）。
	 *
	 * 经 NetworkPhysics sim action 以 Authority 风格下发：服务器单方面决定，目标客户端不做预测。
	 * 目标玩家的输入包只有其本地端能写，跨 Pawn 施力无法走输入包路径，只能由此下发。
	 * 仅在目标 Owner 具备 authority 时生效，且要求目标挂有 UNetworkPhysicsComponent（ChaosMover 后端）。
	 *
	 * @param bOverrideVelocity true 覆盖目标当前速度（强度稳定可预测），false 叠加到当前速度
	 * @param bScheduleForSync  true 按 EventSchedulingMinDelaySeconds 延迟调度，各端在同一物理帧生效、
	 *                          目标端无回滚修正，代价是同等长度的生效延迟；false 立即下发，无延迟但目标端会被修正
	 * @return 成功入队返回 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Launch")
	static UE_API bool ApplyAuthoritativeVelocityToTarget(UMoverComponent* MoverComp,
	                                               FVector          WorldSpaceVelocity,
	                                               bool             bOverrideVelocity = true,
	                                               bool             bScheduleForSync  = true);

	/**
	 * 扫描贴身抓取目标：取当前速度方向前方扇形内、最近的一个带 ASC 的 Pawn。
	 *
	 * 方向基准取速度而非视线——抓取由冲刺撞上触发，判定的是「撞到」而非「瞄准」，
	 * 擦身而过不应命中。判定在发起端做一次即定，落位点随输入包上传，服务器不重算：
	 * 目标位置在发起端是插值外推的结果，两端各自扫描不保证得到同一落点。
	 *
	 * 落位点与目标等高，且水平取双方胶囊半径之和加间隙——抓取双方的垂直运动靠同一
	 * 运动方程保持同步，一方先落地即破坏守恒，故不允许有高度差；留出间隙则是为了
	 * 两个物理体从不产生穿透，否则接触约束会把贴住的双方持续推开。
	 *
	 * @param SearchRadius      搜索半径（cm）
	 * @param HalfAngleDegrees  与速度方向的夹角在此以内才算命中
	 * @param SurfaceClearance  两胶囊表面之间额外留出的间隙（cm）
	 * @param OutAnchorPoint    发起方应当停靠的落位点（世界空间）
	 * @return 未找到目标时返回 nullptr，Out 参数不写入
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Clinch")
	static UE_API APawn* FindClinchTarget(UMoverComponent* MoverComp,
	                               float            SearchRadius,
	                               float            HalfAngleDegrees,
	                               float            SurfaceClearance,
	                               FVector&         OutAnchorPoint);

	/**
	 * 预扫描贴墙推进的路径折线：自被推方位置探到墙面，再沿墙面逐步采样出一条贴合曲面的折线。
	 *
	 * 整条路径一次算定并随输入上传，此后不再重扫。逐帧沿墙重扫看似更贴合，但扫描结果两端
	 * 必然分歧，且被推方的效果由服务器下发、拿不到发起方的输入包，无从复现一条只有发起方
	 * 在扫的曲线。代价是折线不随墙体移动而更新。
	 *
	 * 折线取的是**发起方**的行进位置（已含离墙间隙与双方身位），被推方的偏移由服务器在
	 * 下发其效果时按被推方的权威位置定出，不在此处计算。
	 *
	 * 采样在下列任一情形提前收尾，此时折线短于理论推进距离，推进随之提前结束：
	 * 前方被阻挡（内凹角、柱子）、侧向探不到墙（墙到头）、法线退化（非竖直面）。
	 *
	 * @param TargetLocation    被推方位置，探墙自此出发
	 * @param WallProbeDistance 探墙距离（cm），超出即认为附近无墙
	 * @param PairSeparation    折线与墙面的距离（cm），须容得下被推方加间隙
	 * @param StepDistance      沿墙采样步长（cm），越小越贴合曲面，代价是点数上限更快耗尽
	 * @param Deceleration      摩擦减速度（cm/s²），与初速共同决定折线需要多长
	 * @param MaxPoints         折线点数上限，内部再按输入包的上限收紧
	 * @param OutPath           采样所得折线（世界空间）
	 * @param OutEntrySpeed     速度在墙面内的投影长度（cm/s）
	 * @return 未找到墙面或折线不足两点时返回 false，Out 参数不保证有效
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|WallGrind")
	static UE_API bool TraceWallGrindPath(UMoverComponent* MoverComp,
	                                      FVector          TargetLocation,
	                                      float            WallProbeDistance,
	                                      float            PairSeparation,
	                                      float            StepDistance,
	                                      float            Deceleration,
	                                      int32            MaxPoints,
	                                      TArray<FVector>& OutPath,
	                                      float&           OutEntrySpeed);

	/**
	 * 向目标 MoverComponent 施加 FLayeredMove_LinearVelocity 持续线速度效果。
	 * AS 无法直接构造 TSharedPtr<FLayeredMoveBase>，通过此函数桥接。
	 * @param Velocity          世界空间速度向量（cm/s），持续时间内每帧施加
	 * @param DurationMs        持续时间（毫秒）；0 = 单帧，< 0 = 需手动结束
	 * @param bOverrideVelocity true 覆盖本帧提议速度（屏蔽方向输入/摩擦，直线冲刺）；false 叠加到正常移动
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Launch")
	static UE_API void QueueLinearVelocityMove(UMoverComponent* MoverComp, FVector Velocity, float DurationMs, bool bOverrideVelocity = false);

	/** 获取 SyncState 世界空间位置（自动处理 MovementBase 坐标变换）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|SyncState")
	static UE_API FVector GetSyncStateLocation_WorldSpace(const FMoverDefaultSyncState& SyncState);

	/**
	 * 从输入命令包获取（或创建）FCharacterDefaultInputs 副本。
	 * AS 无法直接调用模板方法 FindOrAddMutableDataByType，通过此函数桥接。
	 * 修改后须调用 CommitDefaultInputs 写回；前一个 ability 写入的字段不会丢失。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|AbilityInput")
	static UE_API FCharacterDefaultInputs GetOrAddDefaultInputs(UPARAM(ref) FMoverInputCmdContext& InputCmd);

	/** 将修改后的 FCharacterDefaultInputs 写回输入命令包。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|AbilityInput")
	static UE_API void CommitDefaultInputs(UPARAM(ref) FMoverInputCmdContext& InputCmd, const FCharacterDefaultInputs& Inputs);

	/**
	 * 从输入命令包获取（或创建）FYanCharacterInputs 副本。
	 * 修改后须调用 CommitYanInputs 写回。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|AbilityInput")
	static UE_API FYanCharacterInputs GetOrAddYanInputs(UPARAM(ref) FMoverInputCmdContext& InputCmd);

	/** 将修改后的 FYanCharacterInputs 写回输入命令包。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|AbilityInput")
	static UE_API void CommitYanInputs(UPARAM(ref) FMoverInputCmdContext& InputCmd, const FYanCharacterInputs& Inputs);

	/**
	 * 以服务器权威方式把目标 Pawn 传送到指定位姿，用于传送门一类的世界 Actor。
	 *
	 * 经 NetworkPhysics sim action 以 Authority 风格下发，走 ChaosMover 的 AttemptTeleport：
	 * 目标点被占据等情况会被 CanTeleport 拒绝，此时 Pawn 原地不动、出口速度也不施加。
	 * 目标 Owner 须具备 authority 且挂有 UNetworkPhysicsComponent（ChaosMover 后端）。
	 *
	 * @param TargetLocation        传送目标位置（世界空间）
	 * @param TargetRotation        传送后的朝向（世界空间）
	 * @param bOverrideVelocity     true 则传送成功后把速度覆盖为 VelocityAfterTeleport，false 保留按旋转差转向后的入口速度
	 * @param VelocityAfterTeleport 出口速度（世界空间，cm/s），仅 bOverrideVelocity 为 true 时生效
	 * @param bScheduleForSync      同 ApplyAuthoritativeVelocityToTarget
	 * @return 成功入队返回 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Teleport")
	static UE_API bool ApplyAuthoritativeTeleportToTarget(UMoverComponent* MoverComp,
	                                               FVector          TargetLocation,
	                                               FRotator         TargetRotation,
	                                               bool             bOverrideVelocity     = false,
	                                               FVector          VelocityAfterTeleport = FVector::ZeroVector,
	                                               bool             bScheduleForSync      = true);

	/**
	 * 以服务器权威方式让目标 Mover 在 DurationMs 内快速位移到 TargetLocation（FYanLayeredMove_BlinkTo），视觉上连续。
	 *
	 * 与 ApplyRadialAttractionToTarget 同为 Authority sim action：位移由别人发起，目标端的输入包不归本端写。
	 * 起点取服务器当前位置；按 EventSchedulingMinDelaySeconds 延迟调度后的实际起点略有出入，
	 * BlinkTo 按位置差反推速度，首帧即收敛。终点须由调用方预先扫描收敛（见 SweepBlinkTarget）。
	 *
	 * @param ForceMovementMode 位移期间切换的移动模式，默认 Falling 以避开地面摩擦与台阶处理
	 * @return 成功入队返回 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Blink")
	static UE_API bool ApplyBlinkToTarget(UMoverComponent* MoverComp,
	                                      FVector          TargetLocation,
	                                      float            DurationMs,
	                                      float            EaseExponent      = 0.5f,
	                                      FName            ForceMovementMode = FName("Falling"));

	/**
	 * 以服务器权威方式向目标 Mover 施加一段径向牵引，用于苍这类持续拉扯的效果。
	 *
	 * 与 ApplyAuthoritativeVelocityToTarget 同为 Authority sim action，区别在于下发的是
	 * 一个持续若干毫秒的 LayeredMove 而非单帧速度：牵引须在目标接近或远离的过程中持续起作用。
	 *
	 * 引力中心以坐标传入而非 Actor：LayeredMove 在物理线程求值，那里访问 AActor 不安全。
	 * 中心会移动时由调用方按刷新间隔反复下发，DurationMs 取略大于间隔即可无缝衔接。
	 *
	 * @param AttractionCenter 引力中心（世界空间）
	 * @param Radius           作用半径（cm），超出完全无牵引
	 * @param Magnitude        中心处的牵引速度（cm/s）
	 * @param FalloffExponent  衰减指数，作用于 (1 - 距离/半径)；1 为线性，越大越集中在近处
	 * @param DurationMs       持续时间（毫秒）
	 * @param bIsPush          true 推开、false 拉近
	 * @return 成功入队返回 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Launch")
	static UE_API bool ApplyRadialAttractionToTarget(UMoverComponent* MoverComp,
	                                          FVector          AttractionCenter,
	                                          float            Radius,
	                                          float            Magnitude,
	                                          float            FalloffExponent = 1.f,
	                                          float            DurationMs      = 200.f,
	                                          bool             bIsPush         = false);

	/**
	 * Authority 刷新同一来源的径向牵引，保留模拟侧进入速度与收拢进度，不重复入队活动 LayeredMove。
	 * Settings.SourceId 必须非零、UpdateSequence 必须递增；Radius <= 0 取消该来源，DurationMs 是刷新后的存续时长。
	 * 中心快照的模拟时间由本接口填写。仅适用于具备 NetworkPhysicsComponent 的目标。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Attraction")
	static UE_API bool RefreshRadialAttractionToTarget(UMoverComponent* MoverComp, const FYanLayeredMove_RadialAttraction& Settings, bool bScheduleForSync = true);

	/**
	 * 以服务器权威方式把被钩住的 Pawn 朝汇合点牵引，与发起者侧的 UChaosGrapplingMode 构成对称的两端。
	 *
	 * 与 ApplyRadialAttractionToTarget 同为 Authority sim action，区别在于下发的牵引复用
	 * 钩锁那一套积分（减半的钩锁重力 + 主动拉力 + 合速度限速），且为覆盖式，
	 * 被钩者自身的下落与地面摩擦不再把牵引吃掉。
	 *
	 * 汇合点每帧都在动，由调用方按刷新间隔反复下发，DurationMs 取略大于间隔即可无缝衔接。
	 *
	 * @param MeetPoint        牵引汇合点（世界空间），取自发起方输入包的 GrappleAnchorPoint
	 * @param PullAcceleration 朝汇合点的主动拉力加速度（cm/s^2）
	 * @param MaxPullSpeed     合速度上限（cm/s）
	 * @param StopDistance     距汇合点在此以内即不再施力（cm）
	 * @param DurationMs       持续时间（毫秒）
	 * @return 成功入队返回 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Grappling")
	static UE_API bool ApplyGrapplePullToTarget(UMoverComponent* MoverComp,
	                                     FVector          MeetPoint,
	                                     float            PullAcceleration = 3000.f,
	                                     float            MaxPullSpeed     = 2500.f,
	                                     float            StopDistance     = 80.f,
	                                     float            DurationMs       = 200.f);

	/**
	 * 以服务器权威方式让目标在一段时间内无法落地，用于击飞这类不该被地面摩擦接管的位移。
	 *
	 * 下发一个只携带 Mover.DisableLanding 标签的 LayeredMove：首帧把目标踢进 Falling，
	 * 此后仅以标签压住落地判定，到期自动失效。与击飞速度分开下发，两者时长可各自调。
	 *
	 * @param DurationSeconds 禁止落地的持续时间（秒）
	 * @return 成功入队返回 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Launch")
	static UE_API bool ApplyNoLandingToTarget(UMoverComponent* MoverComp, float DurationSeconds = 0.3f);

	/**
	 * 以服务器权威方式钉住被抓方并扳正其朝向，与发起方共同构成贴身抓取。
	 *
	 * 与发起方那一份效果的差别只在参数：被抓方无须位移（落位点即其自身当前位置），
	 * 但朝向被接管。下落时刻 FallStartSimTimeMs 必须与发起方取自同一来源——
	 * 本效果经 sim action 下发，生效时刻本就晚于发起方一个往返，
	 * 各自计时会让高度差随时间线性拉大，双方须在同一绝对时刻一同起落。
	 *
	 * 挣脱不走本通道：被抓方凭自身输入包中的 bWantsToBreakFree 当帧解除，无需等待往返。
	 *
	 * @param AnchorLocation     被抓方的落位点（世界空间），取其发起帧位置
	 * @param FallStartSimTimeMs 双方共同的下落起始时刻（绝对仿真时间，毫秒）
	 * @param TargetYaw          被抓方被扳向的朝向（度，世界空间）
	 * @param MaxDurationSeconds 存续上限（秒），到期强制解除以免异常状态下被永久钉住
	 * @return 成功入队返回 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Clinch")
	static UE_API bool ApplyClinchToTarget(UMoverComponent* MoverComp,
	                                FVector          AnchorLocation,
	                                double           FallStartSimTimeMs,
	                                float            TargetYaw,
	                                float            MaxHoldSpeed       = 600.f,
	                                float            TurnRateDegrees    = 720.f,
	                                float            MaxDurationSeconds = 8.f);

	/**
	 * 解除被抓状态：移除目标身上所有授予 Status.Grabbed 的效果，恢复其余技能的激活权。
	 *
	 * 只解 GAS 那一层。移动上的钉制由 FYanLayeredMove_Clinch 承载，须由挣脱者
	 * 在自身输入包写入 bWantsToBreakFree 当帧解除——两层的解除路径本就不同，
	 * 前者要服务器权威，后者要本地即时响应。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Clinch")
	static UE_API void ReleaseGrabbedState(AActor* TargetActor);

	/**
	 * 通过 GAS GameplayEffect 对目标施加击飞效果，由发起者 ASC 向目标 ASC 应用。
	 * 内部使用 SetByCaller 将速度分量编码进 GE Spec，由 ExecCalc 解码后落到目标 Mover。
	 * 持续时长、移动模式等施加方式配在 UMoverLaunchGameplayEffect 上，不经此处传入。
	 *
	 * @param InstigatorASC     发起者 AbilitySystemComponent（客户端调用；GAS 自动转服务器执行）
	 * @param TargetASC         目标 AbilitySystemComponent
	 * @param LaunchEffectClass 击飞效果类，直接用 UMoverLaunchGameplayEffect 或其蓝图子类；留空即不施加
	 * @param LaunchVelocity    世界空间击飞速度（cm/s）
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Launch")
	static UE_API void ApplyLaunchEffectToTarget(UAbilitySystemComponent*     InstigatorASC,
	                                      UAbilitySystemComponent*     TargetASC,
	                                      TSubclassOf<UGameplayEffect> LaunchEffectClass,
	                                      FVector                      LaunchVelocity);
};

#undef UE_API
