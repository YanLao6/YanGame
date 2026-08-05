#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "ChaosMover/ChaosMovementModeTransition.h"
#include "CharacterSlideCheck.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * UCharacterSlideCheck - enter-slide transition (mounted on Walking mode).
 *
 * All three conditions must be met simultaneously:
 *   1. Character is on a walkable floor (no air-slide).
 *   2. FYanCharacterInputs.bWantsToSlide == true.
 *   3. Horizontal speed >= MinSlideSpeed.
 *   4. Jump input is NOT pressed this frame.
 */
UCLASS(MinimalAPI, Blueprintable, EditInlineNew, DefaultToInstanced)
class UCharacterSlideCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UE_API UCharacterSlideCheck(const FObjectInitializer& InitializerModule);

	/** Minimum horizontal speed (cm/s) required to enter slide */
	UPROPERTY(EditDefaultsOnly, Category = "Slide", meta = (ClampMin = "0", UIMin = "0"))
	float MinSlideSpeed = 1000.0f;

	/** 进入滑铲瞬间沿移动方向叠加的前冲速度大小（cm/s），以 Additive 冲量一次性施加 */
	UPROPERTY(EditDefaultsOnly, Category = "Slide", meta = (ClampMin = "0", UIMin = "0"))
	float SlideBoostSpeed = 200.0f;

	//~Begin UBaseMovementModeTransition Interface
	UE_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	UE_API virtual void                  Trigger_Implementation(const FSimulationTickParams& Params) override;
	//~End UBaseMovementModeTransition Interface
};

#undef UE_API
