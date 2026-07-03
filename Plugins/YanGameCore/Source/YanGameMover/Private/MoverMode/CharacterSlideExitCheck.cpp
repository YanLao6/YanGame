#include "MoverMode/CharacterSlideExitCheck.h"

#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "YanMoverDataModelTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CharacterSlideExitCheck)

UCharacterSlideExitCheck::UCharacterSlideExitCheck(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	bSupportsAsync = true;
}

FTransitionEvalResult UCharacterSlideExitCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	const FYanCharacterInputs* YanInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();
	if (YanInputs && YanInputs->bWantsToSlide)
	{
		if (const FMoverDefaultSyncState* SyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
		{
			const FVector Velocity = SyncState->GetVelocity_WorldSpace();
			if (FVector(Velocity.X, Velocity.Y, 0.f).Size() < SlideEndSpeed)
			{
				return FTransitionEvalResult(FName("Walking"));
			}
		}
	}
	else
	{
		return FTransitionEvalResult(FName("Walking"));
	}

	return FTransitionEvalResult::NoTransition;
}
