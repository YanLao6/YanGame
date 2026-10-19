#pragma once

#include "CoreMinimal.h"
#include "LayeredMove.h"
#include "LayeredMoveBase.h"

#include "YanGrapplePullMove.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 被钩者一侧的钩锁牵引：把被钩住的 Pawn 朝汇合点拉过去。
 *
 * 与发起者侧的 UChaosGrapplingMode::GenerateMove 是同一套积分——钩锁重力、朝汇合点的
 * 主动拉力、合速度限速——双方的弧线才对称。发起者走运动模式而被钩者走 LayeredMove，
 * 是因为被钩者的输入包只有其本地端能写，跨 Pawn 施力只能由服务器经 sim action 下发。
 *
 * 覆盖式而非叠加式：被钩者若仍保有正常移动，其自身的下落与地面摩擦会把牵引吃掉大半，
 * 且全额重力会盖过减半后的钩锁重力，「牵引期间重力变小」对被钩者就不成立了。
 *
 * 汇合点以坐标而非 Actor 给出：本结构在物理线程求值，那里访问 AActor 不安全。
 * 汇合点每帧都在动，由 UYanGrappleAuthorityComponent 按刷新间隔反复下发，
 * DurationMs 取略大于间隔即可无缝衔接。
 */
USTRUCT(BlueprintType)
struct FYanLayeredMove_GrapplePull : public FLayeredMoveBase
{
	GENERATED_BODY()

	UE_API FYanLayeredMove_GrapplePull();

	virtual ~FYanLayeredMove_GrapplePull() = default;

	/** 牵引汇合点（世界空间），取自发起方输入包的 GrappleAnchorPoint */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector MeetPoint = FVector::ZeroVector;

	/** 朝汇合点的主动拉力加速度（cm/s^2） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float PullAcceleration = 3000.0f;

	/** 合速度上限（cm/s），对含重力与拉力的总速度做限制 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxPullSpeed = 2500.0f;

	/**
	 * 距汇合点在此以内即不再施加拉力（cm）。
	 * 双方相向收敛时会先后越过汇合点，继续施力只会让两人隔着中点反复对冲。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = "0", ForceUnits = "cm"))
	float StopDistance = 80.0f;

	//~Begin FLayeredMoveBase Interface
	virtual bool SupportsAsync() const override { return true; }

	UE_API virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	UE_API virtual bool GenerateMove_Async(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;

	UE_API virtual FLayeredMoveBase* Clone() const override;
	UE_API virtual void NetSerialize(FArchive& Ar) override;
	UE_API virtual UScriptStruct* GetScriptStruct() const override;
	UE_API virtual FString ToSimpleString() const override;
	UE_API virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
	//~End FLayeredMoveBase Interface
};

template <>
struct TStructOpsTypeTraits<FYanLayeredMove_GrapplePull> : public TStructOpsTypeTraitsBase2<FYanLayeredMove_GrapplePull>
{
	enum
	{
		WithCopy = true
	};
};

#undef UE_API
