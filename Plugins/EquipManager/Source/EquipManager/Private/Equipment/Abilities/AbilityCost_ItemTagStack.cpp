// Copyright Epic Games, Inc. All Rights Reserved.

#include "Equipment/Abilities/AbilityCost_ItemTagStack.h"

#include "Equipment/Abilities/GameplayAbility_FromEquipment.h"
#include "Inventory/InventoryItemInstance.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityCost_ItemTagStack)

UE_DEFINE_GAMEPLAY_TAG(TAG_ABILITY_FAIL_COST_ITEM_TAG_STACK, "Ability.ActivateFail.Cost");

UAbilityCost_ItemTagStack::UAbilityCost_ItemTagStack()
{
	Quantity.SetValue(1.0f);
	FailureTag = TAG_ABILITY_FAIL_COST_ITEM_TAG_STACK;
}

bool UAbilityCost_ItemTagStack::CheckCost(const UModularGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	const UGameplayAbility_FromEquipment* EquipmentAbility = Cast<const UGameplayAbility_FromEquipment>(Ability);
	if (!EquipmentAbility)
	{
		return false;
	}

	UInventoryItemInstance* ItemInstance = EquipmentAbility->GetAssociatedItem();
	if (!ItemInstance)
	{
		return false;
	}

	const int32 NumStacks     = FMath::TruncToInt(Quantity.GetValueAtLevel(Ability->GetAbilityLevel(Handle, ActorInfo)));
	const bool  bCanApplyCost = ItemInstance->GetStatTagStackCount(Tag) >= NumStacks;

	// 回传失败标签，让其他技能与 UI 得知拒绝原因
	if (!bCanApplyCost && OptionalRelevantTags && FailureTag.IsValid())
	{
		OptionalRelevantTags->AddTag(FailureTag);
	}

	return bCanApplyCost;
}

void UAbilityCost_ItemTagStack::ApplyCost(const UModularGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	// 库存扣减只在服务端进行，客户端等待复制结果
	if (!ActorInfo->IsNetAuthority())
	{
		return;
	}

	const UGameplayAbility_FromEquipment* EquipmentAbility = Cast<const UGameplayAbility_FromEquipment>(Ability);
	if (!EquipmentAbility)
	{
		return;
	}

	if (UInventoryItemInstance* ItemInstance = EquipmentAbility->GetAssociatedItem())
	{
		const int32 NumStacks = FMath::TruncToInt(Quantity.GetValueAtLevel(Ability->GetAbilityLevel(Handle, ActorInfo)));
		ItemInstance->RemoveStatTagStack(Tag, NumStacks);
	}
}
