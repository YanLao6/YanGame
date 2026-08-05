#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "ChaosMover/ChaosMovementModeTransition.h"
#include "CharacterSprintCheck.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 *
 */
UCLASS(MinimalAPI, Blueprintable, EditInlineNew, DefaultToInstanced)
class UCharacterSprintCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UE_API UCharacterSprintCheck(const FObjectInitializer& ObjectInitializer);

	/** Dash impulse magnitude (cm/s), added on top of current velocity */
	UPROPERTY(EditDefaultsOnly, Category = "Sprint", meta = (ClampMin = "0", UIMin = "0"))
	float DashImpulseSpeed = 1000.0f;

	//~Begin UBaseMovementModeTransition Interface
	UE_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	UE_API virtual void                  Trigger_Implementation(const FSimulationTickParams& Params) override;
	//~End UBaseMovementModeTransition Interface
};

#undef UE_API
