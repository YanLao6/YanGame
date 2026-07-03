#include "MoverMode/ChaosCharacterSprintCheck.h"

#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "YanMoverDataModelTypes.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/Character/ChaosCharacterMoverComponent.h"
#include "ChaosMover/Character/Effects/ChaosCharacterApplyVelocityEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosCharacterSprintCheck)

UChaosCharacterSprintCheck::UChaosCharacterSprintCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;

	// Re-enter Walking→Walking so that Trigger() is invoked; evaluated only on the first sub-step
	// to prevent multiple impulses within a single Mover update tick.
	bAllowModeReentry = true;
	bFirstSubStepOnly = true;
}

FTransitionEvalResult UChaosCharacterSprintCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
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

void UChaosCharacterSprintCheck::Trigger_Implementation(const FSimulationTickParams& Params)
{
	UChaosCharacterMoverComponent* MoverComp = GetMoverComponent<UChaosCharacterMoverComponent>();
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
	TSharedPtr<FChaosCharacterApplyVelocityEffect> Effect = MakeShared<FChaosCharacterApplyVelocityEffect>();
	Effect->VelocityOrImpulseToApply                      = DashDir * DashImpulseSpeed;
	Effect->Mode                                          = EChaosMoverVelocityEffectMode::OverrideVelocity;
	Simulation->QueueInstantMovementEffect_Internal(Effect);

	// 通知游戏线程此 ChaosMoveEffect 事件（动画/音效/GAS 用）
	Simulation->AddEvent(MakeShared<FSprintEventData>(Params.TimeStep, DashDir * DashImpulseSpeed));
}
