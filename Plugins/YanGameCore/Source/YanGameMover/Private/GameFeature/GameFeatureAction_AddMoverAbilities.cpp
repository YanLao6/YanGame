// Fill out your copyright notice in the Description page of Project Settings.

#include "GameFeature/GameFeatureAction_AddMoverAbilities.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFeaturesSubsystem.h"
#include "MoverComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "ModularPlayerState.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureAction_AddMoverAbilities)

#define LOCTEXT_NAMESPACE "GameFeatures"

// ── UGameFeatureAction_AddMoverAbilities ─────────────────────────────────────

void UGameFeatureAction_AddMoverAbilities::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	FPerContextData& ActiveData = ContextData.FindOrAdd(Context);

	if (!ensureAlways(ActiveData.ActiveExtensions.IsEmpty()) || !ensureAlways(ActiveData.ComponentRequests.IsEmpty()))
	{
		Reset(ActiveData);
	}
	Super::OnGameFeatureActivating(Context);
}

void UGameFeatureAction_AddMoverAbilities::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);
	FPerContextData* ActiveData = ContextData.Find(Context);

	if (ensure(ActiveData))
	{
		Reset(*ActiveData);
	}
}

#if WITH_EDITOR
EDataValidationResult UGameFeatureAction_AddMoverAbilities::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	int32 EntryIndex = 0;
	for (const FGameFeatureMoverEntry& Entry : MoversList)
	{
		if (Entry.ActorClass.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				LOCTEXT("MoverEntryNullActor", "MoversList[{0}] ActorClass 为空"),
				FText::AsNumber(EntryIndex)));
		}

		if (Entry.GrantedMoverAbilitySets.IsEmpty())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				LOCTEXT("MoverEntryNoSets", "MoversList[{0}] GrantedMoverAbilitySets 为空，该条目不会产生任何效果"),
				FText::AsNumber(EntryIndex)));
		}

		int32 SetIndex = 0;
		for (const TSoftObjectPtr<const UMoverAbilitySet>& SetPtr : Entry.GrantedMoverAbilitySets)
		{
			if (SetPtr.IsNull())
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(FText::Format(
					LOCTEXT("MoverSetNull", "MoversList[{0}].GrantedMoverAbilitySets[{1}] 为空"),
					FText::AsNumber(EntryIndex),
					FText::AsNumber(SetIndex)));
			}
			++SetIndex;
		}
		++EntryIndex;
	}

	return Result;
}
#endif

void UGameFeatureAction_AddMoverAbilities::AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld*          World        = WorldContext.World();
	UGameInstance*   GameInstance = WorldContext.OwningGameInstance;
	FPerContextData& ActiveData   = ContextData.FindOrAdd(ChangeContext);

	if ((GameInstance != nullptr) && (World != nullptr) && World->IsGameWorld())
	{
		if (UGameFrameworkComponentManager* ComponentMan = UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance))
		{
			int32 EntryIndex = 0;
			for (const FGameFeatureMoverEntry& Entry : MoversList)
			{
				if (!Entry.ActorClass.IsNull())
				{
					UGameFrameworkComponentManager::FExtensionHandlerDelegate Delegate =
					UGameFrameworkComponentManager::FExtensionHandlerDelegate::CreateUObject(
						this,
						&UGameFeatureAction_AddMoverAbilities::HandleActorExtension,
						EntryIndex,
						ChangeContext);
					TSharedPtr<FComponentRequestHandle> Handle = ComponentMan->AddExtensionHandler(Entry.ActorClass, Delegate);
					ActiveData.ComponentRequests.Add(Handle);
					++EntryIndex;
				}
			}
		}
	}
}

void UGameFeatureAction_AddMoverAbilities::Reset(FPerContextData& ActiveData)
{
	while (!ActiveData.ActiveExtensions.IsEmpty())
	{
		auto It = ActiveData.ActiveExtensions.CreateIterator();
		RemoveActorMoverAbilities(It->Key, ActiveData);
	}
	ActiveData.ComponentRequests.Empty();
}

void UGameFeatureAction_AddMoverAbilities::HandleActorExtension(AActor* Actor, FName EventName, int32 EntryIndex, FGameFeatureStateChangeContext ChangeContext)
{
	FPerContextData* ActiveData = ContextData.Find(ChangeContext);
	if (!MoversList.IsValidIndex(EntryIndex) || !ActiveData)
	{
		return;
	}

	const FGameFeatureMoverEntry& Entry = MoversList[EntryIndex];
	if ((EventName == UGameFrameworkComponentManager::NAME_ExtensionRemoved) || (EventName == UGameFrameworkComponentManager::NAME_ReceiverRemoved))
	{
		RemoveActorMoverAbilities(Actor, *ActiveData);
	}
	else if ((EventName == UGameFrameworkComponentManager::NAME_ExtensionAdded) || (EventName == UGameFrameworkComponentManager::NAME_GameActorReady))
	{
		AddActorMoverAbilities(Actor, Entry, *ActiveData);
	}
}

void UGameFeatureAction_AddMoverAbilities::AddActorMoverAbilities(AActor* Actor, const FGameFeatureMoverEntry& Entry, FPerContextData& ActiveData)
{
	check(Actor);
	if (!Actor->HasAuthority())
	{
		return;
	}

	if (ActiveData.ActiveExtensions.Contains(Actor))
	{
		return;
	}

	UMoverComponent* MoverComp = Actor->FindComponentByClass<UMoverComponent>();
	if (!MoverComp)
	{
		UE_LOG(LogGameFeatures, Warning, TEXT("[AddMoverAbilities] %s 未找到 UMoverComponent，跳过。"), *Actor->GetPathName());
		return;
	}

	FActorMoverExtensions AddedExtensions;
	AddedExtensions.MoverSetHandles.Reserve(Entry.GrantedMoverAbilitySets.Num());

	for (const TSoftObjectPtr<const UMoverAbilitySet>& SetPtr : Entry.GrantedMoverAbilitySets)
	{
		if (const UMoverAbilitySet* Set = SetPtr.Get())
		{
			Set->GiveTo(MoverComp, &AddedExtensions.MoverSetHandles.AddDefaulted_GetRef());
		}
	}

	ActiveData.ActiveExtensions.Add(Actor, MoveTemp(AddedExtensions));
}

void UGameFeatureAction_AddMoverAbilities::RemoveActorMoverAbilities(AActor* Actor, FPerContextData& ActiveData)
{
	FActorMoverExtensions* Extensions = ActiveData.ActiveExtensions.Find(Actor);
	if (!Extensions)
	{
		return;
	}

	UMoverComponent* MoverComp = Actor->FindComponentByClass<UMoverComponent>();

	for (FMoverAbilitySet_GrantedHandles& Handle : Extensions->MoverSetHandles)
	{
		Handle.TakeFrom(MoverComp);
	}

	ActiveData.ActiveExtensions.Remove(Actor);
}

#undef LOCTEXT_NAMESPACE
