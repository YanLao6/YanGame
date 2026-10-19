// Copyright Chronicler.

#include "GameFeature/GameFeatureAction_TagRelationship.h"

#include "AbilitySystemGlobals.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "ModularAbilityPlayerState.h"
#include "ActorComponent/ModularAbilitySystemComponent.h"
#include "GameplayAbilities/ModularAbilityTagRelationshipMapping.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureAction_TagRelationship)

#define LOCTEXT_NAMESPACE "GameFeatures"

void UGameFeatureAction_TagRelationship::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	FPerContextData& ActiveData = ContextData.FindOrAdd(Context);

	if (!ensureAlways(ActiveData.AppliedMappings.IsEmpty()) ||
	    !ensureAlways(ActiveData.ExtensionRequests.IsEmpty()))
	{
		Reset(ActiveData);
	}
	Super::OnGameFeatureActivating(Context);
}

void UGameFeatureAction_TagRelationship::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	if (FPerContextData* ActiveData = ContextData.Find(Context); ensure(ActiveData))
	{
		Reset(*ActiveData);
	}
}

#if WITH_EDITOR
EDataValidationResult UGameFeatureAction_TagRelationship::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	for (int32 EntryIndex = 0; EntryIndex < TagRelationshipList.Num(); ++EntryIndex)
	{
		const FGameFeatureTagRelationshipEntry& Entry = TagRelationshipList[EntryIndex];
		if (Entry.ActorClass.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("TagRelationshipNullActor", "Null ActorClass at index {0} in TagRelationshipList"), FText::AsNumber(EntryIndex)));
		}

		if (Entry.TagRelationshipMapping.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("TagRelationshipNullMapping", "Null TagRelationshipMapping at index {0} in TagRelationshipList"), FText::AsNumber(EntryIndex)));
		}
	}

	return Result;
}
#endif

void UGameFeatureAction_TagRelationship::AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld*        World        = WorldContext.World();
	UGameInstance* GameInstance = WorldContext.OwningGameInstance;
	if (!GameInstance || !World || !World->IsGameWorld())
	{
		return;
	}

	UGameFrameworkComponentManager* ComponentMan = UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance);
	if (!ComponentMan)
	{
		return;
	}

	FPerContextData& ActiveData = ContextData.FindOrAdd(ChangeContext);
	for (int32 EntryIndex = 0; EntryIndex < TagRelationshipList.Num(); ++EntryIndex)
	{
		const FGameFeatureTagRelationshipEntry& Entry = TagRelationshipList[EntryIndex];
		if (Entry.ActorClass.IsNull())
		{
			continue;
		}

		const UGameFrameworkComponentManager::FExtensionHandlerDelegate Delegate = UGameFrameworkComponentManager::FExtensionHandlerDelegate::CreateUObject(
			this, &ThisClass::HandleActorExtension, EntryIndex, ChangeContext);
		ActiveData.ExtensionRequests.Add(ComponentMan->AddExtensionHandler(Entry.ActorClass, Delegate));
	}
}

void UGameFeatureAction_TagRelationship::Reset(FPerContextData& ActiveData)
{
	for (const FAppliedMapping& Applied : ActiveData.AppliedMappings)
	{
		if (UModularAbilitySystemComponent* ModularASC = Applied.ASC.Get())
		{
			ModularASC->RemoveTagRelationshipMapping(Applied.Mapping);
		}
	}

	ActiveData.AppliedMappings.Empty();
	ActiveData.ExtensionRequests.Empty();
}

void UGameFeatureAction_TagRelationship::HandleActorExtension(AActor* Actor, FName EventName, int32 EntryIndex, FGameFeatureStateChangeContext ChangeContext)
{
	FPerContextData* ActiveData = ContextData.Find(ChangeContext);
	if (!ActiveData || !TagRelationshipList.IsValidIndex(EntryIndex))
	{
		return;
	}

	if (EventName == UGameFrameworkComponentManager::NAME_ExtensionRemoved || EventName == UGameFrameworkComponentManager::NAME_ReceiverRemoved)
	{
		RemoveMappings(Actor, *ActiveData);
	}
	else if (EventName == UGameFrameworkComponentManager::NAME_ExtensionAdded
	         || EventName == UGameFrameworkComponentManager::NAME_ReceiverAdded
	         || EventName == AModularAbilityPlayerState::NAME_ModularAbilityReady)
	{
		// ASC 可能不在接收者自身（例如 Pawn 的 ASC 挂在 PlayerState 上），故多个时机都尝试，AddMapping 自行去重。
		AddMapping(Actor, EntryIndex, *ActiveData);
	}
}

void UGameFeatureAction_TagRelationship::AddMapping(AActor* Actor, int32 EntryIndex, FPerContextData& ActiveData)
{
	check(Actor);

	const bool bAlreadyApplied = ActiveData.AppliedMappings.ContainsByPredicate([Actor, EntryIndex](const FAppliedMapping& Applied)
	{
		return Applied.Actor.Get() == Actor && Applied.EntryIndex == EntryIndex;
	});
	if (bAlreadyApplied)
	{
		return;
	}

	UModularAbilitySystemComponent* ModularASC = Cast<UModularAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor));
	if (!ModularASC)
	{
		return;
	}

	const UModularAbilityTagRelationshipMapping* Mapping = TagRelationshipList[EntryIndex].TagRelationshipMapping.LoadSynchronous();
	if (!Mapping)
	{
		return;
	}

	ModularASC->AddTagRelationshipMapping(Mapping);
	ActiveData.AppliedMappings.Add({Actor, EntryIndex, ModularASC, Mapping});
}

void UGameFeatureAction_TagRelationship::RemoveMappings(AActor* Actor, FPerContextData& ActiveData)
{
	ActiveData.AppliedMappings.RemoveAll([Actor](const FAppliedMapping& Applied)
	{
		if (Applied.Actor.Get() != Actor)
		{
			return false;
		}

		if (UModularAbilitySystemComponent* ModularASC = Applied.ASC.Get())
		{
			ModularASC->RemoveTagRelationshipMapping(Applied.Mapping);
		}
		return true;
	});
}

#undef LOCTEXT_NAMESPACE
