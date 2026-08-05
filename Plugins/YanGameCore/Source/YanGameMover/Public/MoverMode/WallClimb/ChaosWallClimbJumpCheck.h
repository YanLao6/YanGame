#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/ChaosMovementModeTransition.h"
#include "MoverSimulationTypes.h"

#include "ChaosWallClimbJumpCheck.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * UChaosWallClimbJumpCheck - 爬墙跳，挂在 UChaosWallClimbMode 上。
 *
 * 竖直分量恒为 JumpUpwardsSpeed（先抵消既有竖直速度，使起跳高度稳定）。
 * 水平分量由视角水平前向 V 与墙面外法线水平分量 N 决定，设二者夹角为 θ：
 *  - 正对墙面（θ=180°）水平分量为零，起跳纯竖直，可沿同一面墙持续向上攀升
 *  - 视角转向侧面时水平分量朝该侧增长，至 θ=90° 达到最大
 *  - 视角转向墙外（θ<90°）水平分量维持最大并随视角偏转
 * 实现为 V·N ≥ 0 时取 Max·V，否则取 Max·(V-(V·N)N)，后者模长恰为 Max·sinθ，
 * 两段在 θ=90° 处连续，无需插值曲线。
 *
 * 视角背离墙面时虽然无法主动移动，但仍可起跳。
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UChaosWallClimbJumpCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UE_API UChaosWallClimbJumpCheck(const FObjectInitializer& ObjectInitializer);

	/** 起跳后切换到的运动模式 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb")
	FName TransitionToMode = FName("Falling");

	/** 起跳的竖直速度（cm/s） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Jump", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float JumpUpwardsSpeed = 700.f;

	/** 起跳水平分量的最大速度（cm/s），在视角与墙面平行或朝外时取到 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Jump", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxHorizontalSpeed = 600.f;

	//~Begin UBaseMovementModeTransition Interface
	/** 跳跃键按下时切换到 TransitionToMode */
	UE_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;

	/** 计算并施加起跳速度 */
	UE_API virtual void Trigger_Implementation(const FSimulationTickParams& Params) override;
	//~End UBaseMovementModeTransition Interface
};

#undef UE_API
