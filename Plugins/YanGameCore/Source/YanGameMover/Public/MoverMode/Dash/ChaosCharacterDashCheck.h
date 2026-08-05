#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "MoverSimulationTypes.h"
#include "ChaosMover/ChaosMovementModeTransition.h"
#include "ChaosCharacterDashCheck.generated.h"

#define UE_API YANGAMEMOVER_API

USTRUCT()
struct FDashEventData : public FMoverSimulationEventData
{
	GENERATED_BODY()

	FDashEventData(const FMoverTimeStep& InEventTime, FVector InDashVelocity)
		: FMoverSimulationEventData(InEventTime)
		  , DashVelocity(InDashVelocity)
	{}

	FDashEventData() {}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FDashEventData::StaticStruct();
	}

	FVector DashVelocity = FVector::ZeroVector;
};

/**
 * 一次性定向冲刺：按下瞬间沿输入方向以固定速度覆盖移动一小段时间。
 *
 * 由脉冲位 FYanCharacterInputs::bIsSprintJustPressed 触发，速度经
 * FLayeredMove_LinearVelocity 以 OverrideVelocity 施加，玩家在此期间
 * 仍可转向但无法改变速率。
 */
UCLASS(MinimalAPI, Blueprintable, EditInlineNew, DefaultToInstanced)
class UChaosCharacterDashCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UE_API UChaosCharacterDashCheck(const FObjectInitializer& ObjectInitializer);

	/** 冲刺速度大小（cm/s），持续期间覆盖正常移动速度 */
	UPROPERTY(EditDefaultsOnly, Category = "Dash", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float DashSpeed = 1000.0f;

	/** 冲刺持续时间（毫秒），LayeredMove 在 Mover 仿真内自行计时 */
	UPROPERTY(EditDefaultsOnly, Category = "Dash", meta = (ClampMin = "0", UIMin = "0"))
	float DashDurationMs = 100.0f;

	//~Begin UBaseMovementModeTransition Interface
	UE_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	UE_API virtual void                  Trigger_Implementation(const FSimulationTickParams& Params) override;
	//~End UBaseMovementModeTransition Interface
};

#undef UE_API
