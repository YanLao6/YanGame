#include "MoverMode/CharacterSlideCheck.h"

#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "YanMoverDataModelTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CharacterSlideCheck)

UCharacterSlideCheck::UCharacterSlideCheck(const FObjectInitializer& InitializerModule)
	:Super(InitializerModule)
{
	bSupportsAsync = true;
}

FTransitionEvalResult UCharacterSlideCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	if (const UMoverBlackboard* Blackboard = Params.SimBlackboard)
	{
		FFloorCheckResult FloorResult;
		if (Blackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult) && FloorResult.IsWalkableFloor())
		{
			const FCharacterDefaultInputs* DefaultInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
			if (DefaultInputs && !DefaultInputs->bIsJumpJustPressed)
			{
				const FYanCharacterInputs* YanInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();
				if (YanInputs && YanInputs->bWantsToSlide)
				{
					if (const FMoverDefaultSyncState* SyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
					{
						const FVector Velocity = SyncState->GetVelocity_WorldSpace();
						if (FVector(Velocity.X, Velocity.Y, 0.f).Size() > MinSlideSpeed)
						{
							return FTransitionEvalResult(FName("Sliding"));
						}
					}
				}
			}
		}
	}

	return FTransitionEvalResult::NoTransition;
}
