// Copyright Epic Games, Inc. All Rights Reserved.

#include "Equipment/Abilities/AbilityCost_InventoryItem.h"

#include "GameFramework/Controller.h"
#include "GameplayAbilities/ModularGameplayAbility.h"
#include "GameplayAbilitySpec.h"
#include "GameplayAbilitySpecHandle.h"
#include "Inventory/InventoryItemDefinition.h"
#include "Inventory/InventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityCost_InventoryItem)

UAbilityCost_InventoryItem::UAbilityCost_InventoryItem()
{
	Quantity.SetValue(1.0f);
}

bool UAbilityCost_InventoryItem::CheckCost(const UModularGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ItemDefinition)
	{
		return false;
	}

	const AController* PC = Ability->GetControllerFromActorInfo();
	if (!PC)
	{
		return false;
	}

	const UInventoryManagerComponent* InventoryComponent = PC->FindComponentByClass<UInventoryManagerComponent>();
	if (!InventoryComponent)
	{
		return false;
	}

	const int32 NumItemsToConsume = FMath::TruncToInt(Quantity.GetValueAtLevel(Ability->GetAbilityLevel(Handle, ActorInfo)));

	return InventoryComponent->GetTotalItemCountByDefinition(ItemDefinition) >= NumItemsToConsume;
}

void UAbilityCost_InventoryItem::ApplyCost(const UModularGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	// 库存扣减只在服务端进行，客户端等待复制结果
	if (!ActorInfo->IsNetAuthority() || !ItemDefinition)
	{
		return;
	}

	const AController* PC = Ability->GetControllerFromActorInfo();
	if (!PC)
	{
		return;
	}

	if (UInventoryManagerComponent* InventoryComponent = PC->FindComponentByClass<UInventoryManagerComponent>())
	{
		const int32 NumItemsToConsume = FMath::TruncToInt(Quantity.GetValueAtLevel(Ability->GetAbilityLevel(Handle, ActorInfo)));
		InventoryComponent->ConsumeItemsByDefinition(ItemDefinition, NumItemsToConsume);
	}
}
