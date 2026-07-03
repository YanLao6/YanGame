#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "ChaosMover/ChaosMovementModeTransition.h"
#include "CharacterSlideCheck.generated.h"

/**
 * UCharacterSlideCheck - enter-slide transition (mounted on Walking mode).
 *
 * All three conditions must be met simultaneously:
 *   1. Character is on a walkable floor (no air-slide).
 *   2. FYanCharacterInputs.bWantsToSlide == true.
 *   3. Horizontal speed >= MinSlideSpeed.
 *   4. Jump input is NOT pressed this frame.
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced)
class YANGAMEMOVER_API UCharacterSlideCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UCharacterSlideCheck(const FObjectInitializer& InitializerModule);

	/** Minimum horizontal speed (cm/s) required to enter slide */
	UPROPERTY(EditDefaultsOnly, Category = "Slide", meta = (ClampMin = "0", UIMin = "0"))
	float MinSlideSpeed = 1000.0f;

	//~Begin UBaseMovementModeTransition Interface
	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	//~End UBaseMovementModeTransition Interface
};
