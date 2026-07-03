// Fill out your copyright notice in the Description page of Project Settings.


#include "Phase/ModularGamePhaseSubsystem.h"

#include "GameplayAbilitySpec.h"
#include "ActorComponent/ModularAbilitySystemComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Phase/ModularGamePhaseAbility.h"
#include "Phase/ModularGamePhaseLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularGamePhaseSubsystem)

class UModularGameplayAbility;
class UObject;

DEFINE_LOG_CATEGORY(LogModularGamePhase);

//////////////////////////////////////////////////////////////////////
// UModularGamePhaseSubsystem

UModularGamePhaseSubsystem::UModularGamePhaseSubsystem()
{}

void UModularGamePhaseSubsystem::PostInitialize()
{
	Super::PostInitialize();
}

bool UModularGamePhaseSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (Super::ShouldCreateSubsystem(Outer))
	{
		//UWorld* World = Cast<UWorld>(Outer);
		//check(World);

		//return World->GetAuthGameMode() != nullptr;
		//return nullptr;
		return true;
	}

	return false;
}

bool UModularGamePhaseSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UModularGamePhaseSubsystem::StartPhase(TSubclassOf<UModularGamePhaseAbility> PhaseAbility, FModularGamePhaseDelegate PhaseEndedCallback)
{
	UWorld*                         World         = GetWorld();
	UModularAbilitySystemComponent* GameState_ASC = World->GetGameState()->FindComponentByClass<UModularAbilitySystemComponent>();
	if (ensure(GameState_ASC))
	{
		FGameplayAbilitySpec       PhaseSpec(PhaseAbility, 1, 0, this);
		FGameplayAbilitySpecHandle SpecHandle = GameState_ASC->GiveAbilityAndActivateOnce(PhaseSpec);
		FGameplayAbilitySpec*      FoundSpec  = GameState_ASC->FindAbilitySpecFromHandle(SpecHandle);

		if (FoundSpec && FoundSpec->IsActive())
		{
			FModularGamePhaseEntry& Entry = ActivePhaseMap.FindOrAdd(SpecHandle);
			Entry.PhaseEndedCallback      = PhaseEndedCallback;
		}
		else
		{
			PhaseEndedCallback.ExecuteIfBound(nullptr);
		}
	}
}

void UModularGamePhaseSubsystem::K2_StartPhase(TSubclassOf<UModularGamePhaseAbility> PhaseAbility, const FModularGamePhaseDynamicDelegate& PhaseEndedDelegate)
{
	const FModularGamePhaseDelegate EndedDelegate = FModularGamePhaseDelegate::CreateWeakLambda(const_cast<UObject*>(PhaseEndedDelegate.GetUObject()),
	                                                                                            [PhaseEndedDelegate](const UModularGamePhaseAbility* PhaseAbility)
	                                                                                            {
		                                                                                            PhaseEndedDelegate.ExecuteIfBound(PhaseAbility);
	                                                                                            });

	StartPhase(PhaseAbility, EndedDelegate);
}

void UModularGamePhaseSubsystem::K2_WhenPhaseStartsOrIsActive(FGameplayTag PhaseTag, EPhaseTagMatchType MatchType, FModularGamePhaseTagDynamicDelegate WhenPhaseActive)
{
	const FModularGamePhaseTagDelegate ActiveDelegate = FModularGamePhaseTagDelegate::CreateWeakLambda(WhenPhaseActive.GetUObject(),
	                                                                                                   [WhenPhaseActive](const FGameplayTag& PhaseTag)
	                                                                                                   {
		                                                                                                   WhenPhaseActive.ExecuteIfBound(PhaseTag);
	                                                                                                   });

	WhenPhaseStartsOrIsActive(PhaseTag, MatchType, ActiveDelegate);
}

void UModularGamePhaseSubsystem::K2_WhenPhaseEnds(FGameplayTag PhaseTag, EPhaseTagMatchType MatchType, FModularGamePhaseTagDynamicDelegate WhenPhaseEnd)
{
	const FModularGamePhaseTagDelegate EndedDelegate = FModularGamePhaseTagDelegate::CreateWeakLambda(WhenPhaseEnd.GetUObject(),
	                                                                                                  [WhenPhaseEnd](const FGameplayTag& PhaseTag)
	                                                                                                  {
		                                                                                                  WhenPhaseEnd.ExecuteIfBound(PhaseTag);
	                                                                                                  });

	WhenPhaseEnds(PhaseTag, MatchType, EndedDelegate);
}

void UModularGamePhaseSubsystem::WhenPhaseStartsOrIsActive(FGameplayTag PhaseTag, EPhaseTagMatchType MatchType, const FModularGamePhaseTagDelegate& WhenPhaseActive)
{
	FPhaseObserver Observer;
	Observer.PhaseTag      = PhaseTag;
	Observer.MatchType     = MatchType;
	Observer.PhaseCallback = WhenPhaseActive;
	PhaseStartObservers.Add(Observer);

	if (IsPhaseActive(PhaseTag))
	{
		WhenPhaseActive.ExecuteIfBound(PhaseTag);
	}
}

void UModularGamePhaseSubsystem::WhenPhaseEnds(FGameplayTag PhaseTag, EPhaseTagMatchType MatchType, const FModularGamePhaseTagDelegate& WhenPhaseEnd)
{
	FPhaseObserver Observer;
	Observer.PhaseTag      = PhaseTag;
	Observer.MatchType     = MatchType;
	Observer.PhaseCallback = WhenPhaseEnd;
	PhaseEndObservers.Add(Observer);
}

bool UModularGamePhaseSubsystem::IsPhaseActive(const FGameplayTag& PhaseTag) const
{
	for (const auto& KVP : ActivePhaseMap)
	{
		const FModularGamePhaseEntry& PhaseEntry = KVP.Value;
		if (PhaseEntry.PhaseTag.MatchesTag(PhaseTag))
		{
			return true;
		}
	}

	return false;
}

void UModularGamePhaseSubsystem::OnBeginPhase(const UModularGamePhaseAbility* PhaseAbility, const FGameplayAbilitySpecHandle PhaseAbilityHandle)
{
	const FGameplayTag IncomingPhaseTag = PhaseAbility->GetGamePhaseTag();

	UE_LOG(LogModularGamePhase, Log, TEXT("Beginning Phase '%s' (%s)"), *IncomingPhaseTag.ToString(), *GetNameSafe(PhaseAbility));

	const UWorld*                   World         = GetWorld();
	UModularAbilitySystemComponent* GameState_ASC = World->GetGameState()->FindComponentByClass<UModularAbilitySystemComponent>();
	if (ensure(GameState_ASC))
	{
		TArray<FGameplayAbilitySpec*> ActivePhases;
		for (const auto& KVP : ActivePhaseMap)
		{
			const FGameplayAbilitySpecHandle ActiveAbilityHandle = KVP.Key;
			if (FGameplayAbilitySpec* Spec = GameState_ASC->FindAbilitySpecFromHandle(ActiveAbilityHandle))
			{
				ActivePhases.Add(Spec);
			}
		}

		for (const FGameplayAbilitySpec* ActivePhase : ActivePhases)
		{
			const UModularGamePhaseAbility* ActivePhaseAbility = CastChecked<UModularGamePhaseAbility>(ActivePhase->Ability);
			const FGameplayTag              ActivePhaseTag     = ActivePhaseAbility->GetGamePhaseTag();

			// So if the active phase currently matches the incoming phase tag, we allow it.
			// i.e. multiple gameplay abilities can all be associated with the same phase tag.
			// For example,
			// You can be in the, Game.Playing, phase, and then start a sub-phase, like Game.Playing.SuddenDeath
			// Game.Playing phase will still be active, and if someone were to push another one, like,
			// Game.Playing.ActualSuddenDeath, it would end Game.Playing.SuddenDeath phase, but Game.Playing would
			// continue.  Similarly if we activated Game.GameOver, all the Game.Playing* phases would end.
			if (!IncomingPhaseTag.MatchesTag(ActivePhaseTag))
			{
				UE_LOG(LogModularGamePhase, Log, TEXT("\tEnding Phase '%s' (%s)"), *ActivePhaseTag.ToString(), *GetNameSafe(ActivePhaseAbility));

				FGameplayAbilitySpecHandle HandleToEnd = ActivePhase->Handle;
				GameState_ASC->CancelAbilitiesByFunc([HandleToEnd](const UModularGameplayAbility* ModularAbility, FGameplayAbilitySpecHandle Handle)
				                                     {
					                                     return Handle == HandleToEnd;
				                                     },
				                                     true);
			}
		}

		FModularGamePhaseEntry& Entry = ActivePhaseMap.FindOrAdd(PhaseAbilityHandle);
		Entry.PhaseTag                = IncomingPhaseTag;

		// Notify all observers of this phase that it has started.
		for (const FPhaseObserver& Observer : PhaseStartObservers)
		{
			if (Observer.IsMatch(IncomingPhaseTag))
			{
				Observer.PhaseCallback.ExecuteIfBound(IncomingPhaseTag);
			}
		}
	}
}

void UModularGamePhaseSubsystem::OnEndPhase(const UModularGamePhaseAbility* PhaseAbility, const FGameplayAbilitySpecHandle PhaseAbilityHandle)
{
	const FGameplayTag EndedPhaseTag = PhaseAbility->GetGamePhaseTag();
	UE_LOG(LogModularGamePhase, Log, TEXT("Ended Phase '%s' (%s)"), *EndedPhaseTag.ToString(), *GetNameSafe(PhaseAbility));

	const FModularGamePhaseEntry& Entry = ActivePhaseMap.FindChecked(PhaseAbilityHandle);
	Entry.PhaseEndedCallback.ExecuteIfBound(PhaseAbility);

	ActivePhaseMap.Remove(PhaseAbilityHandle);

	// Notify all observers of this phase that it has ended.
	for (const FPhaseObserver& Observer : PhaseEndObservers)
	{
		if (Observer.IsMatch(EndedPhaseTag))
		{
			Observer.PhaseCallback.ExecuteIfBound(EndedPhaseTag);
		}
	}
}

bool UModularGamePhaseSubsystem::FPhaseObserver::IsMatch(const FGameplayTag& ComparePhaseTag) const
{
	switch (MatchType)
	{
		case EPhaseTagMatchType::ExactMatch:
			return ComparePhaseTag == PhaseTag;
		case EPhaseTagMatchType::PartialMatch:
			return ComparePhaseTag.MatchesTag(PhaseTag);
	}

	return false;
}
