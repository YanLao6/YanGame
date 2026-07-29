#include "MoverMode/Dash/ChaosCharacterDashCheck.h"

#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "YanMoverDataModelTypes.h"
#include "LayeredMoveBase.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/Character/ChaosCharacterMoverComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosCharacterDashCheck)

UChaosCharacterDashCheck::UChaosCharacterDashCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;

	// Re-enter Walking→Walking so that Trigger() is invoked; evaluated only on the first sub-step
	// to prevent queuing multiple layered moves within a single Mover update tick.
	bAllowModeReentry = true;
	bFirstSubStepOnly = true;
}

FTransitionEvalResult UChaosCharacterDashCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
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

void UChaosCharacterDashCheck::Trigger_Implementation(const FSimulationTickParams& Params)
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

	// 玩家仍可转向（时长由 LayeredMove 在仿真内自行计时）。
	const FVector                           DashVelocity = DashDir * DashSpeed;
	TSharedPtr<FLayeredMove_LinearVelocity> Move         = MakeShared<FLayeredMove_LinearVelocity>();
	Move->Velocity                                       = DashVelocity;
	Move->DurationMs                                     = DashDurationMs;
	Move->MixMode                                        = EMoveMixMode::AdditiveVelocity;
	Simulation->QueueLayeredMove_Internal(Move);

	// 通知游戏线程此冲刺事件（动画/音效/GAS 用）
	Simulation->AddEvent(MakeShared<FDashEventData>(Params.TimeStep, DashVelocity));
}
