// 背包物品发放 PawnData Fragment 实现。

#include "DataAsset/Fragments/PawnDataFragment_AddInventoryItems.h"

#include "Equipment/QuickBarComponent.h"
#include "GameFeaturesSubsystem.h"
#include "GameFramework/Controller.h"
#include "Inventory/InventoryItemDefinition.h"
#include "Inventory/InventoryItemInstance.h"
#include "Inventory/InventoryManagerComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PawnDataFragment_AddInventoryItems)

#define LOCTEXT_NAMESPACE "PawnDataFragment_AddInventoryItems"

UPawnDataFragmentState* UPawnDataFragment_AddInventoryItems::Apply(const FPawnDataFragmentContext& Context) const
{
	AController* Controller = Context.Controller;
	if (!Controller)
	{
		return nullptr;
	}

	UInventoryManagerComponent* Inventory = Controller->FindComponentByClass<UInventoryManagerComponent>();
	if (!Inventory)
	{
		UE_LOG(LogGameFeatures, Error, TEXT("Add Inventory Items Fragment：[%s] 上没有背包组件，物品不会被发放。"), *Controller->GetPathName());
		return nullptr;
	}

	UQuickBarComponent* QuickBar    = Controller->FindComponentByClass<UQuickBarComponent>();
	int32               SlotToEquip = INDEX_NONE;

	UPawnDataFragmentState_AddInventoryItems* State = NewObject<UPawnDataFragmentState_AddInventoryItems>(Context.TargetActor);

	for (const FPawnDataInventoryItemGrant& Grant : GrantedItems)
	{
		TSubclassOf<UInventoryItemDefinition> ItemDef = Grant.ItemDefinition.LoadSynchronous();
		if (!ItemDef)
		{
			continue;
		}

		UInventoryItemInstance* Instance = Inventory->AddItemDefinition(ItemDef, FMath::Max(1, Grant.StackCount));
		if (!Instance)
		{
			UE_LOG(LogGameFeatures, Warning, TEXT("Add Inventory Items Fragment：物品 [%s] 未能加入 [%s] 的背包。"), *ItemDef->GetName(), *Controller->GetPathName());
			continue;
		}

		State->Items.Add(Instance);

		if (Grant.QuickBarSlot < 0)
		{
			continue;
		}

		if (!QuickBar)
		{
			UE_LOG(LogGameFeatures, Warning, TEXT("Add Inventory Items Fragment：[%s] 上没有快捷栏组件，物品 [%s] 只会留在背包中。"), *Controller->GetPathName(), *ItemDef->GetName());
			continue;
		}

		// 配置指定了目标槽位，因此校验该槽位本身而不另找空位。
		if (!QuickBar->CanAddItemToSlot(Grant.QuickBarSlot, Instance))
		{
			UE_LOG(LogGameFeatures, Warning, TEXT("Add Inventory Items Fragment：槽位 %d 无效、已被占用或不接受该物品类别，物品 [%s] 只会留在背包中。"), Grant.QuickBarSlot, *ItemDef->GetName());
			continue;
		}

		QuickBar->AddItemToSlot(Grant.QuickBarSlot, Instance);

		State->OccupiedSlots.Add(Grant.QuickBarSlot);
		SlotToEquip = (SlotToEquip == INDEX_NONE) ? Grant.QuickBarSlot : FMath::Min(SlotToEquip, Grant.QuickBarSlot);
	}

	if (bActivateFirstOccupiedSlot && QuickBar && (SlotToEquip != INDEX_NONE))
	{
		// 激活槽位会触发装备流程，在 Pawn 上生成装备实例与附属 Actor。
		QuickBar->SetActiveSlotIndex(SlotToEquip);
	}

	return State;
}

void UPawnDataFragment_AddInventoryItems::Revoke(const FPawnDataFragmentContext& Context, UPawnDataFragmentState* State) const
{
	UPawnDataFragmentState_AddInventoryItems* TypedState = Cast<UPawnDataFragmentState_AddInventoryItems>(State);
	if (!TypedState)
	{
		return;
	}

	AController* Controller = Context.Controller;
	if (!Controller)
	{
		return;
	}

	// 先清空槽位，激活中的槽位会在此过程中自动卸下对应装备。
	if (UQuickBarComponent* QuickBar = Controller->FindComponentByClass<UQuickBarComponent>())
	{
		for (int32 SlotIndex : TypedState->OccupiedSlots)
		{
			QuickBar->RemoveItemFromSlot(SlotIndex);
		}
	}

	if (UInventoryManagerComponent* Inventory = Controller->FindComponentByClass<UInventoryManagerComponent>())
	{
		for (const TWeakObjectPtr<UInventoryItemInstance>& Item : TypedState->Items)
		{
			if (UInventoryItemInstance* Instance = Item.Get())
			{
				Inventory->RemoveItemInstance(Instance);
			}
		}
	}

	TypedState->OccupiedSlots.Reset();
	TypedState->Items.Reset();
}

#if WITH_EDITOR
EDataValidationResult UPawnDataFragment_AddInventoryItems::IsFragmentValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsFragmentValid(Context);

	if (GrantedItems.IsEmpty())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(LOCTEXT("NoItems", "Add Inventory Items Fragment 未配置任何物品，不会产生任何效果。"));
	}

	TSet<int32> UsedSlots;
	int32       ItemIndex = 0;
	for (const FPawnDataInventoryItemGrant& Grant : GrantedItems)
	{
		if (Grant.ItemDefinition.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("NullItemDefinition", "Add Inventory Items Fragment 的 GrantedItems[{0}] 未指定物品定义。"), FText::AsNumber(ItemIndex)));
		}

		// 同一槽位只能容纳一件物品，重复配置会让后写入的物品静默留在背包中。
		if (Grant.QuickBarSlot >= 0)
		{
			bool bAlreadyUsed = false;
			UsedSlots.Add(Grant.QuickBarSlot, &bAlreadyUsed);
			if (bAlreadyUsed)
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(FText::Format(LOCTEXT("DuplicateSlot", "Add Inventory Items Fragment 的 GrantedItems 中快捷栏槽位 {0} 被重复配置。"), FText::AsNumber(Grant.QuickBarSlot)));
			}
		}

		++ItemIndex;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
