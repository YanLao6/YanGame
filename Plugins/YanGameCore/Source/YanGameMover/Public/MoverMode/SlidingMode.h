#pragma once

#include "CoreMinimal.h"
#include "YanMovementMode.h"
#include "ChaosMover/Character/Modes/ChaosCharacterMovementMode.h"
#include "SlidingMode.generated.h"

/**
 * USlidingMode - slide locomotion mode, inherits from UYanMovementMode (-> UWalkingMode).
 *
 * GenerateMove: bypasses Super; outputs friction-decayed horizontal velocity, ignoring directional input.
 *              Decay formula mirrors WalkingMode braking: dV = (Friction * |V|) * dt
 *              Slope gravity is also accumulated when a valid floor normal is present.
 *
 * SimulationTick: delegates entirely to UWalkingMode (sweep / floor-stick / step / landing / CaptureFinalState).
 */
UCLASS(Blueprintable)
class YANGAMEMOVER_API USlidingMode : public UChaosCharacterMovementMode
{
	GENERATED_BODY()

public:
	USlidingMode(const FObjectInitializer& ObjectInitializer);

	/** Ground friction coefficient (lower = more slippery), analogous to GroundFriction */
	UPROPERTY(EditDefaultsOnly, Category = "Slide")
	float SlideFriction = 0.1f;

	/** Exit slide when horizontal speed (cm/s) drops below this threshold */
	UPROPERTY(EditDefaultsOnly, Category = "Slide")
	float SlideEndSpeed = 100.0f;

	//~Begin UBaseMovementMode Interface
	/** Computes friction-decayed velocity; does NOT call Super to suppress input-driven acceleration */
	virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
	//~End UBaseMovementMode Interface
};
