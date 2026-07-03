#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "YanMoverDataModelTypes.h"

#include "YanMoverAngelscriptLibrary.generated.h"

class AYanHookProjectile;
class UAbilitySystemComponent;
class UGameplayEffect;
class UMoverBlackboard;

/**
 * 供 AngelScript 调用的 Mover 封装（引擎部分类型/方法未对脚本暴露时由此转发）。
 */
UCLASS()
class YANGAMEMOVER_API UYanMoverAngelscriptLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 从 FMoverDataCollection 读取 FMoverDefaultSyncState（存在则 true 并写入 Out）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Data")
	static bool GetDefaultSyncState(const FMoverDataCollection& Collection, FMoverDefaultSyncState& OutSyncState);

	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Data")
	static bool GetDefaultInputs(const FMoverDataCollection& Collection, FCharacterDefaultInputs& OutInputs);

	/** 从输入命令包读取项目自定义输入（存在则 true 并写入 Out）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Data")
	static bool GetYanCharacterInputs(const FMoverDataCollection& Collection, FYanCharacterInputs& OutInputs);

	/** 等价于 FMoverTickEndData::InitForNewFrame（清空 MovementEndState 与 MoveRecord）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|TickEnd")
	static void InitTickEndForNewFrame(UPARAM(ref) FMoverTickEndData& TickEndData);


	// ---- FMovementModeTickEndState ----

	/** 调用 ResetToDefaults，清空 RemainingMs / NextModeName 等。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementModeTickEnd")
	static void ResetMovementModeTickEndState(UPARAM(ref) FMovementModeTickEndState& EndState);

	// ---- FMovementRecord / FMovementSubstep ----

	/** 清空 MovementRecord。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static void ResetMovementRecord(UPARAM(ref) FMovementRecord& Record);

	/** 构造一条 FMovementSubstep（成员在引擎侧未必对脚本暴露字段）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static FMovementSubstep MakeMovementSubstep(FName MoveName, FVector MoveDelta, bool bIsRelevant);

	/** 追加一条 substep。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static void AppendMovementSubstep(UPARAM(ref) FMovementRecord& Record, FMovementSubstep Substep);

	/** 总位移 Delta 累计。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static FVector GetMovementRecordTotalMoveDelta(const FMovementRecord& Record);

	/** 计入速度/状态的「相关」位移部分。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static FVector GetMovementRecordRelevantMoveDelta(const FMovementRecord& Record);

	/** 由相关位移与 DeltaSeconds 推导的速度。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static FVector GetMovementRecordRelevantVelocity(const FMovementRecord& Record);

	/** 拷贝一份 substep 列表（用于调试或遍历）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static TArray<FMovementSubstep> GetMovementRecordSubsteps(const FMovementRecord& Record);

	/** 锁定 relevancy 判定结果。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static void LockMovementRecordRelevancy(UPARAM(ref) FMovementRecord& Record, bool bLockValue);

	/** 解除 relevancy 锁定。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static void UnlockMovementRecordRelevancy(UPARAM(ref) FMovementRecord& Record);

	/** 设置本记录对应的总 Delta 时间（秒）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static void SetMovementRecordDeltaSeconds(UPARAM(ref) FMovementRecord& Record, float DeltaSeconds);

	/** 调试字符串。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static FString MovementRecordToString(const FMovementRecord& Record);

	/** 获取 SyncState 世界空间线速度（自动处理 MovementBase 坐标变换）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|SyncState")
	static FVector GetSyncStateVelocity_WorldSpace(const FMoverDefaultSyncState& SyncState);

	/** 获取 SyncState 世界空间朝向（自动处理 MovementBase 坐标变换）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|SyncState")
	static FRotator GetSyncStateOrientation_WorldSpace(const FMoverDefaultSyncState& SyncState);

	/** 从 MoverComponent 已注册的 Logic 列表中找到指定类型的实例（不存在返回 nullptr）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|LayeredMove")
	static ULayeredMoveLogic* FindRegisteredMoveLogic(UMoverComponent* MoverComp, TSubclassOf<ULayeredMoveLogic> MoveLogicClass);

	/**
	 * 从 MoverBlackboard 读取上一帧地面法线（世界空间 ImpactNormal）。
	 * 无地面数据或地面不可行走时返回 MoverComp 的 UpDirection。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Floor")
	static FVector GetLastFloorNormal(UMoverComponent* MoverComp);

	/** 黑板中是否存在有效的可行走地面记录（IsWalkableFloor() == true）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Floor")
	static bool HasWalkableFloor(UMoverComponent* MoverComp);

	/** LayeredMoveLogic 变体：直接接受 SimBlackboard，内部通过 Outer 推导 MoverComponent。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Floor")
	static bool HasWalkableFloorFromBlackboard(UMoverBlackboard* SimBlackboard);

	/** 从 FCharacterDefaultInputs 获取世界空间移动输入方向（自动处理 MovementBase 坐标变换）。
	 *  AS 无法直接调用非 UFUNCTION 的 GetMoveInput_WorldSpace，通过此函数桥接。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Data")
	static FVector GetMoveInputWorldSpace(const FCharacterDefaultInputs& Inputs);

	/** 向 MoverComponent 请求一次速度冲击（FApplyVelocityEffect）。
	 *  AS 侧无法直接传 CustomThunk 通配结构体，通过此函数桥接。
	 *  @param bAdditive true 则叠加到当前速度，false 则直接覆盖。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Dash")
	static void QueueVelocityImpulse(UMoverComponent* MoverComp, FVector WorldSpaceVelocity, bool bAdditive);

	/** 强制切换 MovementMode（零速度叠加 + ForceMovementMode，保持当前速度不变）。
	 *  钩锁激活时用于确保玩家进入 Falling 模式。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Grappling")
	static void QueueForceMovementMode(UMoverComponent* MoverComp, FName NewModeName);

	/** 从 UMoverBlackboard 的 Outer 推导重力加速度向量。
	 *  ULayeredMoveLogic::GenerateMove 无直接 MoverComp 参数，通过此函数桥接。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Grappling")
	static FVector GetGravityFromBlackboard(UMoverBlackboard* SimBlackboard);

	/**
	 * 获取 Pawn 拥有者的视点世界坐标与前向量，用于发射/瞄准取位。
	 * 原点取 Pawn::GetPawnViewLocation()，方向取上传服务器的输入 ControlRotation：
	 * 两者在客户端与服务器确定性一致，不依赖纯客户端的 PlayerCameraManager。
	 * 失败时（拥有者非 Pawn）返回 false，Out 参数不写入。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Grappling")
	static bool GetOwnerViewLocationAndForward(UMoverComponent* MoverComp, FVector& OutLocation, FVector& OutForward);

	/**
	 * 在指定世界位置和朝向 Spawn AYanHookProjectile，由 MoverComp 提供世界上下文。
	 * 失败时（MoverComp 无效、ProjectileClass 为空、World 无效）返回 nullptr。
	 * 使用 AlwaysSpawn 策略，Owner 设为 MoverComp 的拥有者。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Grappling")
	static AYanHookProjectile* SpawnHookProjectile(UMoverComponent* MoverComp,
	                                                TSubclassOf<AYanHookProjectile> ProjectileClass,
	                                                FVector SpawnLocation,
	                                                FRotator SpawnRotation);

	/**
	 * 从摄像机位置沿前向量做钩锁 LineTrace（WorldStatic 通道），返回是否命中及命中点。
	 * 自动忽略 Pawn 自身。MaxDistance 为本次探测最大距离（cm）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Grappling")
	static bool LineTraceForGrapple(UMoverComponent* MoverComp, float MaxDistance, FVector& OutImpactPoint);

	/**
	 * 向目标 MoverComponent 施加 FLayeredMove_Launch 击飞效果。
	 * AS 无法直接构造 TSharedPtr<FLayeredMoveBase>，通过此函数桥接。
	 * @param LaunchVelocity    击飞速度向量（世界空间，cm/s）
	 * @param ForceMovementMode 发射前强制切换的移动模式（NAME_None 则不切换）
	 * @param DurationMs        持续时间（毫秒），0 = 单帧冲击
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Launch")
	static void QueueLaunchMove(UMoverComponent* MoverComp, FVector LaunchVelocity, FName ForceMovementMode, float DurationMs);

	/**
	 * 向目标 MoverComponent 施加 FLayeredMove_LinearVelocity 持续线速度效果。
	 * AS 无法直接构造 TSharedPtr<FLayeredMoveBase>，通过此函数桥接。
	 * @param Velocity   世界空间速度向量（cm/s），每帧叠加到角色速度上
	 * @param DurationMs 持续时间（毫秒）；0 = 单帧，< 0 = 需手动结束
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Launch")
	static void QueueLinearVelocityMove(UMoverComponent* MoverComp, FVector Velocity, float DurationMs);

	/** 获取 SyncState 世界空间位置（自动处理 MovementBase 坐标变换）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|SyncState")
	static FVector GetSyncStateLocation_WorldSpace(const FMoverDefaultSyncState& SyncState);

	/**
	 * 从输入命令包获取（或创建）FCharacterDefaultInputs 副本。
	 * AS 无法直接调用模板方法 FindOrAddMutableDataByType，通过此函数桥接。
	 * 修改后须调用 CommitDefaultInputs 写回；前一个 ability 写入的字段不会丢失。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|AbilityInput")
	static FCharacterDefaultInputs GetOrAddDefaultInputs(UPARAM(ref) FMoverInputCmdContext& InputCmd);

	/** 将修改后的 FCharacterDefaultInputs 写回输入命令包。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|AbilityInput")
	static void CommitDefaultInputs(UPARAM(ref) FMoverInputCmdContext& InputCmd, const FCharacterDefaultInputs& Inputs);

	/**
	 * 从输入命令包获取（或创建）FYanCharacterInputs 副本。
	 * 修改后须调用 CommitYanInputs 写回。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|AbilityInput")
	static FYanCharacterInputs GetOrAddYanInputs(UPARAM(ref) FMoverInputCmdContext& InputCmd);

	/** 将修改后的 FYanCharacterInputs 写回输入命令包。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|AbilityInput")
	static void CommitYanInputs(UPARAM(ref) FMoverInputCmdContext& InputCmd, const FYanCharacterInputs& Inputs);

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
	static void ApplyLaunchEffectToTarget(UAbilitySystemComponent* InstigatorASC,
	                                      UAbilitySystemComponent* TargetASC,
	                                      TSubclassOf<UGameplayEffect>  LaunchEffectClass,
	                                      FVector                       LaunchVelocity,
	                                      float                         DurationMs);
};
