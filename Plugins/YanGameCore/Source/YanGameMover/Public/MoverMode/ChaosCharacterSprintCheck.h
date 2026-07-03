#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "MoverSimulationTypes.h"
#include "ChaosMover/ChaosMovementModeTransition.h"
#include "ChaosCharacterSprintCheck.generated.h"

USTRUCT()
struct FSprintEventData : public FMoverSimulationEventData
{
	GENERATED_BODY()

	FSprintEventData(const FMoverTimeStep& InEventTime, FVector InDashVelocity)
		: FMoverSimulationEventData(InEventTime)
		  , DashVelocity(InDashVelocity)
	{}

	FSprintEventData() {}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FSprintEventData::StaticStruct();
	}

	FVector DashVelocity = FVector::ZeroVector;
};

/**
 *
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced)
class YANGAMEMOVER_API UChaosCharacterSprintCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UChaosCharacterSprintCheck(const FObjectInitializer& ObjectInitializer);

	/** Dash impulse magnitude (cm/s), added on top of current velocity */
	UPROPERTY(EditDefaultsOnly, Category = "Sprint", meta = (ClampMin = "0", UIMin = "0"))
	float DashImpulseSpeed = 1000.0f;

	//~Begin UBaseMovementModeTransition Interface
	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	virtual void                  Trigger_Implementation(const FSimulationTickParams& Params) override;
	//~End UBaseMovementModeTransition Interface
};
