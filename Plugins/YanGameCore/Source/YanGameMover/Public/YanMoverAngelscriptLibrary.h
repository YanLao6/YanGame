#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "YanMoverDataModelTypes.h"

#include "YanMoverAngelscriptLibrary.generated.h"

#define UE_API YANGAMEMOVER_API

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
	 * 通过 GAS GameplayEffect 对目标施加击飞效果，由发起者 ASC 向目标 ASC 应用。
	 * 内部使用 SetByCaller 将速度分量和持续时间编码进 GE Spec，由 ExecCalc 解码后调用 QueueLaunchMove。
	 * @param InstigatorASC   发起者 AbilitySystemComponent（客户端调用；GAS 自动转服务器执行）
	 * @param TargetASC       目标 AbilitySystemComponent
	 * @param LaunchEffectClass 使用 UYanLaunchExecutionCalculation 的 GameplayEffect 资产类
	 * @param LaunchVelocity  世界空间击飞速度（cm/s）
	 * @param DurationMs      FLayeredMove_Launch 持续时间（毫秒）
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Launch")
	static UE_API void ApplyLaunchEffectToTarget(UAbilitySystemComponent*     InstigatorASC,
	                                      UAbilitySystemComponent*     TargetASC,
	                                      TSubclassOf<UGameplayEffect> LaunchEffectClass,
	                                      FVector                      LaunchVelocity,
	                                      float                        DurationMs);
};

#undef UE_API
