// Copyright Epic Games, Inc. All Rights Reserved.

#include "Equipment/Abilities/GameplayAbility_FromEquipment.h"

#include "Equipment/EquipmentInstance.h"
#include "Equipment/EquipmentManagerComponent.h"
#include "GameFramework/Actor.h"
#include "Inventory/InventoryItemInstance.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayAbility_FromEquipment)

#define LOCTEXT_NAMESPACE "EquipManager"

UGameplayAbility_FromEquipment::UGameplayAbility_FromEquipment(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UEquipmentInstance* UGameplayAbility_FromEquipment::GetAssociatedEquipment() const
{
	UObject* SourceObject = GetAssociatedSourceObject();

	// 被动由装备实例直接授予，来源即实例本身。
	if (UEquipmentInstance* Equipment = Cast<UEquipmentInstance>(SourceObject))
	{
		return Equipment;
	}

	// 主动技能在物品入槽时授予，来源是背包物品：其寿命跨越 Pawn 重生，装备实例则不能，
	// 故只记得住物品，须在此按当前 Avatar 反查生效中的实例。
	const UInventoryItemInstance* SourceItem = Cast<UInventoryItemInstance>(SourceObject);
	if (SourceItem == nullptr)
	{
		return nullptr;
	}

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UEquipmentManagerComponent* EquipmentManager = Avatar ? Avatar->FindComponentByClass<UEquipmentManagerComponent>() : nullptr;
	if (EquipmentManager == nullptr)
	{
		return nullptr;
	}

	// 校验生效实例确由本技能的来源物品驱动，避免切槽后仍拿到别的槽位的装备。
	UEquipmentInstance* ActiveInstance = EquipmentManager->GetActiveInstance();
	if ((ActiveInstance != nullptr) && (ActiveInstance->GetInstigator() == SourceItem))
	{
		return ActiveInstance;
	}

	return nullptr;
}

UInventoryItemInstance* UGameplayAbility_FromEquipment::GetAssociatedItem() const
{
	// 主动技能的来源即物品本身，无需经装备实例回溯。
	if (UInventoryItemInstance* SourceItem = Cast<UInventoryItemInstance>(GetAssociatedSourceObject()))
	{
		return SourceItem;
	}

	if (UEquipmentInstance* Equipment = GetAssociatedEquipment())
	{
		return Cast<UInventoryItemInstance>(Equipment->GetInstigator());
	}

	return nullptr;
}

UObject* UGameplayAbility_FromEquipment::GetAssociatedSourceObject() const
{
	FGameplayAbilitySpec* Spec = UGameplayAbility::GetCurrentAbilitySpec();
	return Spec ? Spec->SourceObject.Get() : nullptr;
}

#if WITH_EDITOR
EDataValidationResult UGameplayAbility_FromEquipment::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 非实例化能力没有独立的 Spec 上下文，GetAssociatedEquipment 将永远失败
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (InstancingPolicy == EGameplayAbilityInstancingPolicy::NonInstanced)
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	{
		Context.AddError(LOCTEXT("EquipmentAbilityMustBeInstanced", "装备能力必须是实例化的（不能使用 NonInstanced 策略）"));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
