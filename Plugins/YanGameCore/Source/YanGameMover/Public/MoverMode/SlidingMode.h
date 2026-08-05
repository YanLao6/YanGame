#pragma once

#include "CoreMinimal.h"
#include "YanMovementMode.h"
#include "ChaosMover/Character/Modes/ChaosCharacterMovementMode.h"
#include "SlidingMode.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * USlidingMode - slide locomotion mode, inherits from UYanMovementMode (-> UWalkingMode).
 *
 * GenerateMove: bypasses Super; outputs friction-decayed horizontal velocity, ignoring directional input.
 *              Decay formula mirrors WalkingMode braking: dV = (Friction * |V|) * dt
 *              Slope gravity is also accumulated when a valid floor normal is present.
 *
 * SimulationTick: delegates entirely to UWalkingMode (sweep / floor-stick / step / landing / CaptureFinalState).
 */
UCLASS(MinimalAPI, Blueprintable)
class USlidingMode : public UChaosCharacterMovementMode
{
	GENERATED_BODY()

public:
	UE_API USlidingMode(const FObjectInitializer& ObjectInitializer);

	/** Ground friction coefficient (lower = more slippery), analogous to GroundFriction */
	UPROPERTY(EditDefaultsOnly, Category = "Slide")
	float SlideFriction = 0.1f;

	/** 正对下坡方向滑行时的摩擦系数（越小越滑），随移动方向与坡向夹角插值 */
	UPROPERTY(EditDefaultsOnly, Category = "Slide", meta = (ClampMin = "0", UIMin = "0"))
	float DownhillFriction = 0.4f;

	/** 正对上坡方向滑行时的摩擦系数（越大越难上坡），随移动方向与坡向夹角插值 */
	UPROPERTY(EditDefaultsOnly, Category = "Slide", meta = (ClampMin = "0", UIMin = "0"))
	float UphillFriction = 3.0f;

	/** 地面法线与上方向的夹角超过该值（度）即视为斜面，改用上/下坡插值摩擦 */
	UPROPERTY(EditDefaultsOnly, Category = "Slide", meta = (ClampMin = "0", UIMin = "0", ClampMax = "90", UIMax = "90", ForceUnits = "degrees"))
	float SlopeAngleThreshold = 5.0f;

	/** Exit slide when horizontal speed (cm/s) drops below this threshold */
	UPROPERTY(EditDefaultsOnly, Category = "Slide")
	float SlideEndSpeed = 100.0f;

	//~Begin UBaseMovementMode Interface
	/** Computes friction-decayed velocity; does NOT call Super to suppress input-driven acceleration */
	UE_API virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
	//~End UBaseMovementMode Interface
};

#undef UE_API
