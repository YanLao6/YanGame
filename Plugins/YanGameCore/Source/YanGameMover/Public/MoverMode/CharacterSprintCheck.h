#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "ChaosMover/ChaosMovementModeTransition.h"
#include "CharacterSprintCheck.generated.h"

/**
 *
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced)
class YANGAMEMOVER_API UCharacterSprintCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UCharacterSprintCheck(const FObjectInitializer& ObjectInitializer);

	/** Dash impulse magnitude (cm/s), added on top of current velocity */
	UPROPERTY(EditDefaultsOnly, Category = "Sprint", meta = (ClampMin = "0", UIMin = "0"))
	float DashImpulseSpeed = 1000.0f;

	//~Begin UBaseMovementModeTransition Interface
	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	virtual void                  Trigger_Implementation(const FSimulationTickParams& Params) override;
	//~End UBaseMovementModeTransition Interface
};
