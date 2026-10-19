// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFeature/GameFeatureAction_AddAbilities.h"
#include "Engine/GameInstance.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif


#include "ModularAbilityPlayerState.h"
#include "ActorComponent/ModularAbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureAction_AddAbilities)

#define LOCTEXT_NAMESPACE "GameFeatures"

//////////////////////////////////////////////////////////////////////
// UGameFeatureAction_AddAbilities

void UGameFeatureAction_AddAbilities::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	FPerContextData& ActiveData = ContextData.FindOrAdd(Context);

	if (!ensureAlways(ActiveData.ActiveExtensions.IsEmpty()) ||
	    !ensureAlways(ActiveData.ComponentRequests.IsEmpty()))
	{
		Reset(ActiveData);
	}
	Super::OnGameFeatureActivating(Context);
}

void UGameFeatureAction_AddAbilities::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);
	FPerContextData* ActiveData = ContextData.Find(Context);

	if (ensure(ActiveData))
	{
		Reset(*ActiveData);
	}
}

#if WITH_EDITOR
EDataValidationResult UGameFeatureAction_AddAbilities::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	int32 EntryIndex = 0;
	for (const FGameFeatureAbilitiesEntry& Entry : AbilitiesList)
	{
		if (Entry.ActorClass.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("EntryHasNullActor", "Null ActorClass at index {0} in AbilitiesList"), FText::AsNumber(EntryIndex)));
		}

		if (Entry.GrantedAbilities.IsEmpty() && Entry.GrantedAttributes.IsEmpty() && Entry.GrantedEffects.IsEmpty() && Entry.GrantedAbilitySets.IsEmpty())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("EntryHasNoAddOns", "Index {0} in AbilitiesList will do nothing (no granted abilities, attributes, effects, or ability sets)"), FText::AsNumber(EntryIndex)));
		}

		int32 AbilityIndex = 0;
		for (const FModularAbilityGrant& Ability : Entry.GrantedAbilities)
		{
			if (Ability.AbilityType.IsNull())
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(FText::Format(LOCTEXT("EntryHasNullAbility", "Null AbilityType at index {0} in AbilitiesList[{1}].GrantedAbilities"), FText::AsNumber(AbilityIndex), FText::AsNumber(EntryIndex)));
			}
			++AbilityIndex;
		}

		int32 AttributesIndex = 0;
		for (const FModularAttributeSetGrant& Attributes : Entry.GrantedAttributes)
		{
			if (Attributes.AttributeSetType.IsNull())
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(FText::Format(LOCTEXT("EntryHasNullAttributeSet", "Null AttributeSetType at index {0} in AbilitiesList[{1}].GrantedAttributes"), FText::AsNumber(AttributesIndex), FText::AsNumber(EntryIndex)));
			}
			++AttributesIndex;
		}

		int32 EffectIndex = 0;
		for (const FModularEffectGrant& Effect : Entry.GrantedEffects)
		{
			if (Effect.EffectType.IsNull())
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(FText::Format(LOCTEXT("EntryHasNullEffect", "Null EffectType at index {0} in AbilitiesList[{1}].GrantedEffects"), FText::AsNumber(EffectIndex), FText::AsNumber(EntryIndex)));
			}
			++EffectIndex;
		}

		int32 AttributeSetIndex = 0;
		for (const TSoftObjectPtr<const UModularAbilitySet>& AttributeSetPtr : Entry.GrantedAbilitySets)
		{
			if (AttributeSetPtr.IsNull())
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(FText::Format(LOCTEXT("EntryHasNullAttributeSet", "Null AbilitySet at index {0} in AbilitiesList[{1}].GrantedAbilitySets"), FText::AsNumber(AttributeSetIndex), FText::AsNumber(EntryIndex)));
			}
			++AttributeSetIndex;
		}
		++EntryIndex;
	}

	return Result;
}
#endif

void UGameFeatureAction_AddAbilities::AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld*          World        = WorldContext.World();
	UGameInstance*   GameInstance = WorldContext.OwningGameInstance;
	FPerContextData& ActiveData   = ContextData.FindOrAdd(ChangeContext);

	if ((GameInstance != nullptr) && (World != nullptr) && World->IsGameWorld())
	{
		if (UGameFrameworkComponentManager* ComponentMan = UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance))
		{
			int32 EntryIndex = 0;
			for (const FGameFeatureAbilitiesEntry& Entry : AbilitiesList)
			{
				if (!Entry.ActorClass.IsNull())
				{
					UGameFrameworkComponentManager::FExtensionHandlerDelegate AddAbilitiesDelegate = UGameFrameworkComponentManager::FExtensionHandlerDelegate::CreateUObject(
						this,
						&UGameFeatureAction_AddAbilities::HandleActorExtension,
						EntryIndex,
						ChangeContext);
					TSharedPtr<FComponentRequestHandle> ExtensionRequestHandle = ComponentMan->AddExtensionHandler(Entry.ActorClass, AddAbilitiesDelegate);

					ActiveData.ComponentRequests.Add(ExtensionRequestHandle);
					EntryIndex++;
				}
			}
		}
	}
}

void UGameFeatureAction_AddAbilities::Reset(FPerContextData& ActiveData)
{
	while (!ActiveData.ActiveExtensions.IsEmpty())
	{
		auto ExtensionIt = ActiveData.ActiveExtensions.CreateIterator();
		RemoveActorAbilities(ExtensionIt->Key, ActiveData);
	}

	ActiveData.ComponentRequests.Empty();
}

void UGameFeatureAction_AddAbilities::HandleActorExtension(AActor* Actor, FName EventName, int32 EntryIndex, FGameFeatureStateChangeContext ChangeContext)
{
	FPerContextData* ActiveData = ContextData.Find(ChangeContext);
	if (AbilitiesList.IsValidIndex(EntryIndex) && ActiveData)
	{
		const FGameFeatureAbilitiesEntry& Entry = AbilitiesList[EntryIndex];
		if ((EventName == UGameFrameworkComponentManager::NAME_ExtensionRemoved) || (EventName == UGameFrameworkComponentManager::NAME_ReceiverRemoved))
		{
			RemoveActorAbilities(Actor, *ActiveData);
		}
		else if (EventName == AModularAbilityPlayerState::NAME_ModularAbilityReady)
		{
			// PawnData 已就绪，直接授予
			AddActorAbilities(Actor, Entry, *ActiveData);
		}
		else if (EventName == UGameFrameworkComponentManager::NAME_ExtensionAdded)
		{
			// 若 Actor 是 ModularAbilityPlayerState 但 PawnData 尚未设置，跳过等 NAME_ModularAbilityReady
			if (const AModularAbilityPlayerState* AbilityPS = Cast<AModularAbilityPlayerState>(Actor))
			{
				if (AbilityPS->GetPawnData<UModularPawnData>() == nullptr)
				{
					return;
				}
			}
			AddActorAbilities(Actor, Entry, *ActiveData);
		}
	}
}

void UGameFeatureAction_AddAbilities::AddActorAbilities(AActor* Actor, const FGameFeatureAbilitiesEntry& AbilitiesEntry, FPerContextData& ActiveData)
{
	check(Actor);
	if (!Actor->HasAuthority())
	{
		return;
	}

	// early out if Actor already has ability extensions applied
	if (ActiveData.ActiveExtensions.Find(Actor) != nullptr)
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = FindOrAddComponentForActor<UAbilitySystemComponent>(Actor, AbilitiesEntry, ActiveData))
	{
		UModularAbilitySystemComponent* ModularASC = CastChecked<UModularAbilitySystemComponent>(AbilitySystemComponent);

		FModularAbilitySet_GrantedHandles& GrantedHandles = ActiveData.ActiveExtensions.Add(Actor);
		ModularAbilityGrant::GiveToAbilitySystem(ModularASC, AbilitiesEntry.GrantedAbilities, AbilitiesEntry.GrantedEffects, AbilitiesEntry.GrantedAttributes, AbilitiesEntry.GrantedAbilitySets, GrantedHandles);
	}
	else
	{
		UE_LOG(LogGameFeatures, Error, TEXT("Failed to find/add an ability component to '%s'. Abilities will not be granted."), *Actor->GetPathName());
	}
}

void UGameFeatureAction_AddAbilities::RemoveActorAbilities(AActor* Actor, FPerContextData& ActiveData)
{
	if (FModularAbilitySet_GrantedHandles* GrantedHandles = ActiveData.ActiveExtensions.Find(Actor))
	{
		if (UModularAbilitySystemComponent* ModularASC = Actor->FindComponentByClass<UModularAbilitySystemComponent>())
		{
			GrantedHandles->TakeFromAbilitySystem(ModularASC);
		}

		ActiveData.ActiveExtensions.Remove(Actor);
	}
}

UActorComponent* UGameFeatureAction_AddAbilities::FindOrAddComponentForActor(UClass* ComponentType, AActor* Actor, const FGameFeatureAbilitiesEntry& AbilitiesEntry, FPerContextData& ActiveData)
{
	UActorComponent* Component = Actor->FindComponentByClass(ComponentType);

	bool bMakeComponentRequest = (Component == nullptr);
	if (Component)
	{
		// Check to see if this component was created from a different `UGameFrameworkComponentManager` request.
		// `Native` is what `CreationMethod` defaults to for dynamically added components.
		if (Component->CreationMethod == EComponentCreationMethod::Native)
		{
			// Attempt to tell the difference between a true native component and one created by the GameFrameworkComponent system.
			// If it is from the UGameFrameworkComponentManager, then we need to make another request (requests are ref counted).
			UObject* ComponentArchetype = Component->GetArchetype();
			bMakeComponentRequest       = ComponentArchetype->HasAnyFlags(RF_ClassDefaultObject);
		}
	}

	if (bMakeComponentRequest)
	{
		UWorld*        World        = Actor->GetWorld();
		UGameInstance* GameInstance = World->GetGameInstance();

		if (UGameFrameworkComponentManager* ComponentMan = UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance))
		{
			TSharedPtr<FComponentRequestHandle> RequestHandle = ComponentMan->AddComponentRequest(AbilitiesEntry.ActorClass, ComponentType);
			ActiveData.ComponentRequests.Add(RequestHandle);
		}

		if (!Component)
		{
			Component = Actor->FindComponentByClass(ComponentType);
			ensureAlways(Component);
		}
	}

	return Component;
}

#undef LOCTEXT_NAMESPACE
