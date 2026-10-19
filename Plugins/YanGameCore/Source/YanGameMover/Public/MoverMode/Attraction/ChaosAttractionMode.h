#pragma once

#include "ChaosMover/Character/Modes/ChaosFlyingMode.h"
#include "ChaosMover/ChaosMovementModeTransition.h"

#include "ChaosAttractionMode.generated.h"

#define UE_API YANGAMEMOVER_API

struct FYanAttractionSource;

/** 从权威牵引状态生成三维飞行输入；Mode 与进入 Transition 均由角色蓝图配置。 */
UCLASS(MinimalAPI, Blueprintable, EditInlineNew, DefaultToInstanced)
class UChaosAttractionMode : public UChaosFlyingMode
{
	GENERATED_BODY()

public:
	/** 接近中心时按距离减小输入幅度，避免满速来回穿过中心。 */
	UPROPERTY(EditAnywhere, Category = "Yan|Mover|Attraction", meta = (ClampMin = "1", ForceUnits = "cm"))
	float SlowdownDistance = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Yan|Mover|Attraction", meta = (ClampMin = "0", ForceUnits = "cm"))
	float StopDistance = 2.0f;

	/** 无有效牵引时返回的模式，可在蓝图中调整。 */
	UPROPERTY(EditAnywhere, Category = "Yan|Mover|Attraction")
	FName ExitMovementMode = DefaultModeNames::Falling;

	/** 按快照与模拟位置计算 [-1,1] 三维输入，期望速度始终受 PullSpeed 与飞行上限约束。 */
	UE_API FVector ComputeMoveInput(const FYanAttractionSource& Source, const FVector& PawnLocation, double SimTimeMs, float FlyingMaxSpeed) const;

	//~Begin UBaseMovementMode Interface
	UE_API virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
	UE_API virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
	//~End UBaseMovementMode Interface
};

/** 配置在允许被牵引的模式或全局 Transitions 上；进入与退出共享同一来源有效性判定。 */
UCLASS(MinimalAPI, Blueprintable, EditInlineNew, DefaultToInstanced)
class UChaosAttractionEnterCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UE_API UChaosAttractionEnterCheck(const FObjectInitializer& ObjectInitializer);

	/** 填写蓝图 MovementModes 中 UChaosAttractionMode 对应的名称。 */
	UPROPERTY(EditAnywhere, Category = "Yan|Mover|Attraction")
	FName TransitionToMode = NAME_None;

	//~Begin UBaseMovementModeTransition Interface
	UE_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	//~End UBaseMovementModeTransition Interface
};

#undef UE_API
