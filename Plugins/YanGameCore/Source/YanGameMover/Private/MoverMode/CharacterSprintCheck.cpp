#include "MoverMode/CharacterSprintCheck.h"

#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "YanMoverDataModelTypes.h"
#include "ChaosMover/ChaosMoverSimulation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CharacterSprintCheck)

UCharacterSprintCheck::UCharacterSprintCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;

	// Re-enter Walking→Walking so that Trigger() is invoked; evaluated only on the first sub-step
	// to prevent multiple impulses within a single Mover update tick.
	bAllowModeReentry = true;
	bFirstSubStepOnly = true;
}

FTransitionEvalResult UCharacterSprintCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	if (UMoverComponent* MoverComp = GetMoverComponent())
	{
		if (const UMoverBlackboard* Blackboard = Simulation->GetBlackboard_Mutable())
		{
			FFloorCheckResult FloorResult;
			if (Blackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult) && FloorResult.IsWalkableFloor())
			{
				if (const FYanCharacterInputs* YanInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>())
				{
					if (YanInputs && YanInputs->bIsSprintJustPressed)
					{
						return FTransitionEvalResult(FName("Walking"));
					}
				}
			}
		}
	}

	return FTransitionEvalResult::NoTransition;
}

void UCharacterSprintCheck::Trigger_Implementation(const FSimulationTickParams& Params)
{
	UMoverComponent* MoverComp = GetMoverComponent();
	check(MoverComp)

	// Prefer the player's current move input direction (world space)
	FVector                        DashDir       = FVector::ZeroVector;
	const FCharacterDefaultInputs* DefaultInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	if (DefaultInputs)
	{
		DashDir = DefaultInputs->GetMoveInput_WorldSpace();
	}

	// Fall back to character's current facing when there is no directional input
	if (DashDir.IsNearlyZero())
	{
		if (DefaultInputs)
		{
			DashDir = DefaultInputs->ControlRotation.Vector();
		}
	}

	// Project onto horizontal plane to avoid gaining vertical velocity on slopes
	DashDir.Z = 0.f;
	if (DashDir.IsNearlyZero())
	{
		return;
	}
	DashDir = DashDir.GetSafeNormal();

	// Inject additive horizontal velocity impulse
	TSharedPtr<FApplyVelocityEffect> Effect = MakeShared<FApplyVelocityEffect>();
	Effect->VelocityToApply                 = DashDir * DashImpulseSpeed;
	Effect->bAdditiveVelocity               = true;
	MoverComp->QueueInstantMovementEffect(Effect);
}
