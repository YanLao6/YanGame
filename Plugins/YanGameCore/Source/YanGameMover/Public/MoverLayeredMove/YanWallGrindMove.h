#pragma once

#include "CoreMinimal.h"
#include "LayeredMove.h"
#include "LayeredMoveBase.h"
#include "NativeGameplayTags.h"

#include "YanWallGrindMove.generated.h"

#define UE_API YANGAMEMOVER_API

/** 贴墙推进状态标签，由本效果在存续期间对外暴露 */
YANGAMEMOVER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mover_IsWallGrinding);

/**
 * 贴墙摩擦推进：把角色送上一条贴合墙面的折线，沿折线匀减速滑行至速度耗尽或走完全程。
 *
 * 推进双方各自持有一份本效果，靠参数区分角色：发起方带一个把自己推离墙面的路径偏移，
 * 被推方偏移近乎为零因而紧贴墙面。二者的运动方程完全相同——与其让一方每帧读取另一方的
 * 位置（对端位置在本地是插值外推的结果，与权威值不符，必然分歧），不如令双方遵循同一
 * 方程各自求值，相对位置因而守恒。
 *
 * 路径在发起帧一次算定并随输入上传，此后不再重扫。逐帧沿墙重扫看似更贴合，
 * 但扫描结果两端必然分歧，且被推方的效果由服务器下发、拿不到发起方的输入包，
 * 无从复现一条只有发起方在扫的曲线。代价是折线不随墙体移动而更新。
 *
 * 偏移定义在随切线旋转的路径坐标系而非世界空间：墙面转弯时，世界空间的常量偏移
 * 会让发起方从被推方身后滑到侧面。
 *
 * 阶段划分：
 *  - 缓动段：自各自的 StartSimTimeMs 起 PullDurationMs 内，从 StartLocation 走向折线起点。
 *    发起方靠它贴上去，被推方原地不动；
 *  - 悬停段：缓动结束至 GrindStartSimTimeMs，锁在折线起点。
 *    这段等待正是两侧生效时差的容纳区间——先到的一方停在原地等，双方才能同时起滑；
 *  - 推进段：自 GrindStartSimTimeMs 起按 s(t) = s0·t − ½·a·t² 求弧长，
 *    位置由弧长在折线上求值，速度由位置差反推，被阻挡时自然收敛而不会积攒动能。
 *
 * 推进结束即终止本效果，角色交回常规移动模式；把被推方甩出由服务器另行调度，不在此处。
 */
USTRUCT(BlueprintType)
struct FYanLayeredMove_WallGrind : public FLayeredMoveBase
{
	GENERATED_BODY()

	UE_API FYanLayeredMove_WallGrind();

	virtual ~FYanLayeredMove_WallGrind() = default;

	/** 贴合墙面的路径折线（世界空间，至少两点）。发起帧算定后不再变更 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	TArray<FVector> Path;

	/** 缓动起点（世界空间），取发起帧的仿真位置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector StartLocation = FVector::ZeroVector;

	/**
	 * 路径坐标系下的常量偏移：X 沿切线、Y 沿外向（背离墙面）、Z 沿世界上。
	 * 发起方取正的 Y 分量把自己顶在被推方身后，被推方取近零偏移紧贴墙面。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector PathFrameOffset = FVector::ZeroVector;

	/** 双方共同的起滑时刻（绝对仿真时间，毫秒），须取自同一来源 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	double GrindStartSimTimeMs = 0.0;

	/** 沿墙初速（cm/s），由发起帧的速度在墙面上的投影取定 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float EntrySpeed = 0.f;

	/** 摩擦减速度（cm/s²）。与初速一同决定推进距离 s0²/(2a) 与时长 s0/a */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = "0"))
	float Deceleration = 900.f;

	/** 缓动段时长（毫秒），决定发起方贴上去的快慢。被推方无须位移，取 0 即可跳过缓动 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = "0", ForceUnits = "ms"))
	float PullDurationMs = 80.f;

	/** 缓动段的时间重映射指数：小于 1 起步快、末端收得缓 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = "0.01"))
	float EaseExponent = 0.5f;

	/** 悬停段回正的速度上限（cm/s），防止被外力推开后以过大速度弹回 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxHoldSpeed = 600.f;

	/** 缓动段与推进段限速：被阻挡而落后于期望位置时，不允许本步速度超过理论值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bRestrictSpeedToExpected = true;

	/** 是否接管朝向。被推方置真以被扳向墙面，发起方置假以保留自由视角 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bControlOrientation = false;

	/** 接管朝向时相对路径切线的偏角（度）：0 为面朝推进方向，90 为面朝墙面 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ForceUnits = "degrees"))
	float YawOffsetFromTangent = 0.f;

	/** 接管朝向时的转向速率上限（度/秒），避免瞬间扭头 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = "1"))
	float TurnRateDegrees = 720.f;

	/**
	 * 被推方专用：凭 FYanCharacterInputs::bWantsToBreakFree 提前结束。
	 * 与发起方分用不同的输入位，否则双方的技能同帧写同一字段时会互相覆盖。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bCanBreakFree = false;

	/**
	 * 发起方专用：推进意图 FYanCharacterInputs::bWantsToWallGrind 落回 false 即结束。
	 * 该意图本就是持续标志，松手即停，无须再为解除单设一个脉冲位。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bEndOnGrindIntentCleared = false;

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

	// 弧长处的落位点：折线上的位置加上路径坐标系偏移；一并给出该处切线供朝向使用
	UE_API FVector ComputeTargetLocation(double Distance, FVector& OutTangent) const;

	// 由「上一步期望位置 → 本步期望位置」反推速度，落后于期望位置时不允许超过理论速度
	UE_API FVector ComputeTrackingVelocity(const FVector& DesiredLocation, const FVector& PreviousDesiredLocation, const FVector& CurrentLocation, float DeltaSeconds) const;

	// 悬停段：一律收敛回折线起点，受 MaxHoldSpeed 限制
	UE_API FVector ComputeHoldVelocity(const FVector& DesiredLocation, const FVector& CurrentLocation, float DeltaSeconds) const;

	// 按切线加偏角求本帧角速度（度/秒），受 TurnRateDegrees 限制
	UE_API FVector ComputeAngularVelocity(const FRotator& CurrentOrientation, const FVector& Tangent, float DeltaSeconds) const;
};

template <>
struct TStructOpsTypeTraits<FYanLayeredMove_WallGrind> : public TStructOpsTypeTraitsBase2<FYanLayeredMove_WallGrind>
{
	enum
	{
		WithCopy = true
	};
};

#undef UE_API
