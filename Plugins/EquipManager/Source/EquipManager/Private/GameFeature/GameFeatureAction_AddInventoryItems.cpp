// 背包物品发放 GameFeatureAction 实现。


#include "GameFeature/GameFeatureAction_AddInventoryItems.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/QuickBarComponent.h"
#include "GameFeaturesSubsystem.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Inventory/InventoryItemDefinition.h"
#include "Inventory/InventoryItemInstance.h"
#include "Inventory/InventoryManagerComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureAction_AddInventoryItems)

#define LOCTEXT_NAMESPACE "GameFeatures"

//~Begin UGameFeatureAction Interface
void UGameFeatureAction_AddInventoryItems::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	FPerContextData& ActiveData = ContextData.FindOrAdd(Context);

	if (!ensureAlways(ActiveData.ActiveExtensions.IsEmpty()) ||
	    !ensureAlways(ActiveData.ComponentRequests.IsEmpty()))
	{
		Reset(ActiveData);
	}

	Super::OnGameFeatureActivating(Context);
}

void UGameFeatureAction_AddInventoryItems::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	if (FPerContextData* ActiveData = ContextData.Find(Context))
	{
		Reset(*ActiveData);
	}
}
//~End UGameFeatureAction Interface

//~Begin UObject Interface
#if WITH_EDITOR
EDataValidationResult UGameFeatureAction_AddInventoryItems::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	int32 EntryIndex = 0;
	for (const FGameFeatureInventoryEntry& Entry : ItemsList)
	{
		if (Entry.ActorClass.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("InventoryEntryHasNullActor", "Null ActorClass at index {0} in ItemsList"), FText::AsNumber(EntryIndex)));
		}

		if (Entry.GrantedItems.IsEmpty())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("InventoryEntryHasNoItems", "Index {0} in ItemsList will do nothing (no granted items)"), FText::AsNumber(EntryIndex)));
		}

		TSet<int32> UsedSlots;
		int32       ItemIndex = 0;
		for (const FInventoryItemGrant& Grant : Entry.GrantedItems)
		{
			if (Grant.ItemDefinition.IsNull())
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(FText::Format(LOCTEXT("InventoryEntryHasNullItem", "Null ItemDefinition at index {0} in ItemsList[{1}].GrantedItems"), FText::AsNumber(ItemIndex), FText::AsNumber(EntryIndex)));
			}

			// 同一槽位只能容纳一件物品，重复配置会让后写入的物品静默丢失。
			if (Grant.QuickBarSlot >= 0)
			{
				bool bAlreadyUsed = false;
				UsedSlots.Add(Grant.QuickBarSlot, &bAlreadyUsed);
				if (bAlreadyUsed)
				{
					Result = EDataValidationResult::Invalid;
					Context.AddError(FText::Format(LOCTEXT("InventoryEntryHasDuplicateSlot", "Duplicate QuickBarSlot {0} in ItemsList[{1}].GrantedItems"), FText::AsNumber(Grant.QuickBarSlot), FText::AsNumber(EntryIndex)));
				}
			}

			++ItemIndex;
		}

		++EntryIndex;
	}

	return Result;
}
#endif
//~End UObject Interface

//~Begin UGameFeatureAction_WorldActionBase Interface
void UGameFeatureAction_AddInventoryItems::AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld*          World        = WorldContext.World();
	UGameInstance*   GameInstance = WorldContext.OwningGameInstance;
	FPerContextData& ActiveData   = ContextData.FindOrAdd(ChangeContext);

	if ((GameInstance == nullptr) || (World == nullptr) || !World->IsGameWorld())
	{
		return;
	}

	if (UGameFrameworkComponentManager* ComponentManager = UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance))
	{
		int32 EntryIndex = 0;
		for (const FGameFeatureInventoryEntry& Entry : ItemsList)
		{
			if (!Entry.ActorClass.IsNull())
			{
				UGameFrameworkComponentManager::FExtensionHandlerDelegate AddItemsDelegate =
					UGameFrameworkComponentManager::FExtensionHandlerDelegate::CreateUObject(
						this,
						&UGameFeatureAction_AddInventoryItems::HandleActorExtension,
						EntryIndex,
						ChangeContext);

				ActiveData.ComponentRequests.Add(ComponentManager->AddExtensionHandler(Entry.ActorClass, AddItemsDelegate));
			}

			++EntryIndex;
		}
	}
}
//~End UGameFeatureAction_WorldActionBase Interface

void UGameFeatureAction_AddInventoryItems::Reset(FPerContextData& ActiveData)
{
	while (!ActiveData.ActiveExtensions.IsEmpty())
	{
		auto ExtensionIt = ActiveData.ActiveExtensions.CreateIterator();
		RemoveItems(ExtensionIt->Key, ActiveData);
	}

	ActiveData.ComponentRequests.Empty();
}

void UGameFeatureAction_AddInventoryItems::HandleActorExtension(AActor* Actor, FName EventName, int32 EntryIndex, FGameFeatureStateChangeContext ChangeContext)
{
	FPerContextData* ActiveData = ContextData.Find(ChangeContext);
	if (!ItemsList.IsValidIndex(EntryIndex) || (ActiveData == nullptr))
	{
		return;
	}

	if ((EventName == UGameFrameworkComponentManager::NAME_ExtensionRemoved) ||
	    (EventName == UGameFrameworkComponentManager::NAME_ReceiverRemoved))
	{
		RemoveItems(Actor, *ActiveData);
		return;
	}

	if ((EventName != UGameFrameworkComponentManager::NAME_ExtensionAdded) &&
	    (EventName != UGameFrameworkComponentManager::NAME_GameActorReady))
	{
		return;
	}

	AController* Controller = Cast<AController>(Actor);
	if ((Controller == nullptr) || !Controller->HasAuthority())
	{
		// 背包写入是服务器权威操作，客户端只接收复制结果。
		return;
	}

	FActorExtensions& Extensions = ActiveData->ActiveExtensions.FindOrAdd(Actor);

	// 同一控制器可能收到多次扩展事件，而同一控制器类又可配置多条条目，因此去重要精确到条目。
	if (Extensions.GrantedEntries.Contains(EntryIndex) || Extensions.PendingEntries.Contains(EntryIndex))
	{
		return;
	}

	if (Controller->GetPawn() != nullptr)
	{
		GrantItems(Controller, EntryIndex, Extensions);
	}
	else
	{
		// 装备链路需要控制器已控制 Pawn，此时只能等待。
		WaitForPawn(Controller, Extensions, EntryIndex, ChangeContext);
	}
}

void UGameFeatureAction_AddInventoryItems::WaitForPawn(AController* Controller, FActorExtensions& Extensions, int32 EntryIndex, FGameFeatureStateChangeContext ChangeContext)
{
	TWeakObjectPtr<AController> WeakController(Controller);

	Extensions.PendingEntries.Add(EntryIndex);
	Extensions.PawnChangedHandles.Add(Controller->GetOnNewPawnNotifier().AddWeakLambda(this,
		[this, WeakController, EntryIndex, ChangeContext](APawn* NewPawn)
		{
			if ((NewPawn == nullptr) || !WeakController.IsValid())
			{
				return;
			}

			FPerContextData* ActiveData = ContextData.Find(ChangeContext);
			if (!ItemsList.IsValidIndex(EntryIndex) || (ActiveData == nullptr))
			{
				return;
			}

			if (FActorExtensions* PendingExtensions = ActiveData->ActiveExtensions.Find(WeakController.Get()))
			{
				if (!PendingExtensions->GrantedEntries.Contains(EntryIndex))
				{
					GrantItems(WeakController.Get(), EntryIndex, *PendingExtensions);
				}
			}
		}));
}

void UGameFeatureAction_AddInventoryItems::GrantItems(AController* Controller, int32 EntryIndex, FActorExtensions& Extensions)
{
	const FGameFeatureInventoryEntry& Entry = ItemsList[EntryIndex];

	UInventoryManagerComponent* Inventory = Controller->FindComponentByClass<UInventoryManagerComponent>();
	if (Inventory == nullptr)
	{
		UE_LOG(LogGameFeatures, Error, TEXT("AddInventoryItems: '%s' 上没有背包组件，物品不会被发放。"), *Controller->GetPathName());
		return;
	}

	UQuickBarComponent* QuickBar     = Controller->FindComponentByClass<UQuickBarComponent>();
	int32               SlotToEquip  = INDEX_NONE;

	for (const FInventoryItemGrant& Grant : Entry.GrantedItems)
	{
		TSubclassOf<UInventoryItemDefinition> ItemDef = Grant.ItemDefinition.LoadSynchronous();
		if (ItemDef == nullptr)
		{
			continue;
		}

		UInventoryItemInstance* Instance = Inventory->AddItemDefinition(ItemDef, FMath::Max(1, Grant.StackCount));
		if (Instance == nullptr)
		{
			UE_LOG(LogGameFeatures, Warning, TEXT("AddInventoryItems: 物品 '%s' 未能加入 '%s' 的背包。"), *ItemDef->GetName(), *Controller->GetPathName());
			continue;
		}

		Extensions.Items.Add(Instance);

		if (Grant.QuickBarSlot < 0)
		{
			continue;
		}

		if (QuickBar == nullptr)
		{
			UE_LOG(LogGameFeatures, Warning, TEXT("AddInventoryItems: '%s' 上没有快捷栏组件，物品 '%s' 只会留在背包中。"), *Controller->GetPathName(), *ItemDef->GetName());
			continue;
		}

		// 授予配置指定了目标槽位，因此校验该槽位本身而不另找空位。
		if (!QuickBar->CanAddItemToSlot(Grant.QuickBarSlot, Instance))
		{
			UE_LOG(LogGameFeatures, Warning, TEXT("AddInventoryItems: 槽位 %d 无效、已被占用或不接受该物品类别，物品 '%s' 只会留在背包中。"), Grant.QuickBarSlot, *ItemDef->GetName());
			continue;
		}

		QuickBar->AddItemToSlot(Grant.QuickBarSlot, Instance);

		Extensions.OccupiedSlots.Add(Grant.QuickBarSlot);
		SlotToEquip = (SlotToEquip == INDEX_NONE) ? Grant.QuickBarSlot : FMath::Min(SlotToEquip, Grant.QuickBarSlot);
	}

	Extensions.PendingEntries.Remove(EntryIndex);
	Extensions.GrantedEntries.Add(EntryIndex);

	if (Entry.bActivateFirstOccupiedSlot && (QuickBar != nullptr) && (SlotToEquip != INDEX_NONE))
	{
		// 激活槽位会触发装备流程，在 Pawn 上生成装备实例与附属 Actor。
		QuickBar->SetActiveSlotIndex(SlotToEquip);
	}
}

void UGameFeatureAction_AddInventoryItems::RemoveItems(AActor* Actor, FPerContextData& ActiveData)
{
	FActorExtensions* Extensions = ActiveData.ActiveExtensions.Find(Actor);
	if (Extensions == nullptr)
	{
		return;
	}

	if (AController* Controller = Cast<AController>(Actor))
	{
		for (const FDelegateHandle& PawnChangedHandle : Extensions->PawnChangedHandles)
		{
			if (PawnChangedHandle.IsValid())
			{
				Controller->GetOnNewPawnNotifier().Remove(PawnChangedHandle);
			}
		}

		// 先清空槽位，激活中的槽位会在此过程中自动卸下对应装备。
		if (UQuickBarComponent* QuickBar = Controller->FindComponentByClass<UQuickBarComponent>())
		{
			for (int32 SlotIndex : Extensions->OccupiedSlots)
			{
				QuickBar->RemoveItemFromSlot(SlotIndex);
			}
		}

		if (UInventoryManagerComponent* Inventory = Controller->FindComponentByClass<UInventoryManagerComponent>())
		{
			for (const TWeakObjectPtr<UInventoryItemInstance>& Item : Extensions->Items)
			{
				if (UInventoryItemInstance* Instance = Item.Get())
				{
					Inventory->RemoveItemInstance(Instance);
				}
			}
		}
	}

	ActiveData.ActiveExtensions.Remove(Actor);
}

#undef LOCTEXT_NAMESPACE
