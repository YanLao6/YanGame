#pragma once

#include "CoreMinimal.h"
#include "LayeredMove.h"
#include "LayeredMoveBase.h"
#include "NativeGameplayTags.h"

#include "YanClinchMove.generated.h"

#define UE_API YANGAMEMOVER_API

/** 贴身状态标签，由本效果在存续期间对外暴露 */
YANGAMEMOVER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mover_IsClinched);

/**
 * 贴身钳制：把角色送到落位点后锁死水平位移，垂直方向照常受重力，直至落地。
 *
 * 抓取双方各自持有一份本效果，靠参数区分角色：发起方缓动到落位点并保留自由视角，
 * 被抓方原地锁定且朝向被扳向发起方。二者的运动方程完全相同，
 * 这正是「相对位置不变」的实现方式——与其让一方每帧读取另一方的位置（对端位置在本地
 * 是插值外推的结果，与权威值不符，必然分歧），不如令双方遵循同一方程各自求值。
 *
 * 垂直同步依赖 FallStartSimTimeMs 这一共同时间基准：发起方本地入队、被抓方经 sim action
 * 延迟下发，两者的生效时刻本就相差一个往返，各自计时必然错开。而下落一旦错开 Δt，
 * 高度差就按 g·Δt·t 随时间线性拉大，百毫秒的偏差一秒后即是近一米。
 * 双方因而在同一绝对时刻开始下落，此后每帧做相同的重力累加，浮点误差亦相同，
 * 高度差恒定。故落位点必须与被抓方等高，一方先落地会立即破坏守恒。
 *
 * 阶段划分：
 *  - 缓动段：自各自的 StartSimTimeMs 起 PullDurationMs 内，从 StartLocation 走向
 *    AnchorLocation，速度由位置差反推，抵达即停。发起方靠它贴上去，被抓方原地不动；
 *  - 悬停段：缓动结束至 FallStartSimTimeMs，锁死在落位点并抵住重力。
 *    这段等待正是两侧生效时差的容纳区间——先到的一方停在原地等，双方才能同时起落；
 *  - 下落段：水平持续收敛回落位点，垂直交由重力累加。
 *    落地由移动模式的碰撞处理终结——地面挡住位移后速度自然归零，无须显式判定，
 *    也就不必查询不参与回滚的黑板地面结果。
 */
USTRUCT(BlueprintType)
struct FYanLayeredMove_Clinch : public FLayeredMoveBase
{
	GENERATED_BODY()

	UE_API FYanLayeredMove_Clinch();

	virtual ~FYanLayeredMove_Clinch() = default;

	/** 缓动起点（世界空间），取发起帧的仿真位置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector StartLocation = FVector::ZeroVector;

	/** 落位点（世界空间）。发起方取「被抓方位置 + 贴身偏移」，被抓方取自身当前位置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector AnchorLocation = FVector::ZeroVector;

	/** 双方共同的下落起始时刻（绝对仿真时间，毫秒），须取自同一来源 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	double FallStartSimTimeMs = 0.0;

	/** 重力加速度向量（cm/s²）。发起时由 sim 取定并随效果下发，物理线程不重算 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector Gravity = FVector(0.f, 0.f, -980.f);

	/** 缓动段时长（毫秒），决定发起方贴上去的快慢。被抓方无须位移，取 0 即可跳过缓动 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = "0", ForceUnits = "ms"))
	float PullDurationMs = 80.f;

	/** 缓动段的时间重映射指数：小于 1 起步快、末端收得缓 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = "0.01"))
	float EaseExponent = 0.5f;

	/** 下落段水平回正的速度上限（cm/s），防止被外力推开后以过大速度弹回 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxHoldSpeed = 600.f;

	/** 缓动段限速：被阻挡而落后于期望位置时，不允许本步速度超过理论值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bRestrictSpeedToExpected = true;

	/** 是否接管朝向。被抓方置真以被扳向发起方，发起方置假以保留自由视角 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bControlOrientation = false;

	/** 接管朝向时的目标 Yaw（度，世界空间） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ForceUnits = "degrees"))
	float TargetYaw = 0.f;

	/** 接管朝向时的转向速率上限（度/秒），避免瞬间扭头 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = "1"))
	float TurnRateDegrees = 720.f;

	/**
	 * 被抓方专用：凭 FYanCharacterInputs::bWantsToBreakFree 提前结束。
	 * 与发起方分用不同的输入位，否则双方的技能同帧写同一字段时会互相覆盖。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bCanBreakFree = false;

	/**
	 * 发起方专用：抓取意图 FYanCharacterInputs::bWantsToClinch 落回 false 即结束。
	 * 该意图本就是持续标志，松手即停，无须再为解除单设一个脉冲位。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bEndOnClinchIntentCleared = false;

	//~Begin FLayeredMoveBase Interface
	virtual bool SupportsAsync() const override { return true; }

	UE_API virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const override;
	UE_API virtual void GetGameplayTags(FGameplayTagContainer& InOutTags) const override;

	UE_API virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	UE_API virtual bool GenerateMove_Async(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;

	UE_API virtual FLayeredMoveBase* Clone() const override;
	UE_API virtual void NetSerialize(FArchive& Ar) override;
	UE_API virtual UScriptStruct* GetScriptStruct() const override;
	UE_API virtual FString ToSimpleString() const override;
	UE_API virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
	//~End FLayeredMoveBase Interface

private:
	// 把线性进度重映射为位移进度，两端须得到完全一致的结果，故只用纯函数
	UE_API float ApplyTimeMapping(float LinearFraction) const;

	// 缓动段：沿 StartLocation → AnchorLocation 求本帧速度
	UE_API FVector ComputePullVelocity(const FVector& CurrentLocation, double ElapsedFromPullStartMs, float DeltaSeconds) const;

	// 悬停段与下落段：水平一律收敛回落位点，垂直分量按 bAllowFall 取重力累加或同样收敛
	UE_API FVector ComputeHoldVelocity(const FVector& CurrentLocation, const FVector& CurrentVelocity, float DeltaSeconds, bool bAllowFall) const;

	// 按 TargetYaw 求本帧角速度（度/秒），受 TurnRateDegrees 限制
	UE_API FVector ComputeAngularVelocity(const FRotator& CurrentOrientation, float DeltaSeconds) const;
};

template <>
struct TStructOpsTypeTraits<FYanLayeredMove_Clinch> : public TStructOpsTypeTraitsBase2<FYanLayeredMove_Clinch>
{
	enum
	{
		WithCopy = true
	};
};

#undef UE_API
