#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "ChaosMover/ChaosMovementModeTransition.h"
#include "CharacterSlideExitCheck.generated.h"

/**
 * UCharacterSlideExitCheck - exit-slide transition (mounted on Sliding mode).
 *
 * Triggers if either condition is met:
 *   1. FYanCharacterInputs.bWantsToSlide == false (player released slide key).
 *   2. Horizontal speed < SlideEndSpeed (natural deceleration stop).
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced)
class YANGAMEMOVER_API UCharacterSlideExitCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UCharacterSlideExitCheck(const FObjectInitializer& ObjectInitializer);

	/** Force exit slide when horizontal speed (cm/s) drops below this value */
	UPROPERTY(EditDefaultsOnly, Category = "Slide", meta = (ClampMin = "0", UIMin = "0"))
	float SlideEndSpeed = 100.0f;

	//~Begin UBaseMovementModeTransition Interface
	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	//~End UBaseMovementModeTransition Interface
};
