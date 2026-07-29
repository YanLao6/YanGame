// 快捷栏组件实现。


#include "Equipment/QuickBarComponent.h"

#include "Equipment/EquipmentDefinition.h"
#include "Equipment/EquipmentInstance.h"
#include "Equipment/EquipmentManagerComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Inventory/InventoryFragment_EquippableItem.h"
#include "Inventory/InventoryItemInstance.h"
#include "ItemDisplay/ItemDisplayTypes.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(QuickBarComponent)


UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EM_QuickBar_Message_SlotsChanged, "EM.QuickBar.Message.SlotsChanged");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EM_QuickBar_Message_ActiveIndexChanged, "EM.QuickBar.Message.ActiveIndexChanged");

// 槽位按键 Tag 的约定前缀，拼上槽位索引即为该槽位的输入 Tag。
static const TCHAR* QuickBarSlotInputTagPrefix = TEXT("InputTag.QuickBar.Slot");

UQuickBarComponent::UQuickBarComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 快捷栏槽位与激活状态都需要同步到客户端。
	SetIsReplicatedByDefault(true);
}

//~Begin UObject Interface
void UQuickBarComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 同步槽位内容和当前激活索引。
	DOREPLIFETIME(ThisClass, Slots);
	DOREPLIFETIME(ThisClass, ActiveSlotIndex);
}
//~End UObject Interface

//~Begin UActorComponent Interface
void UQuickBarComponent::BeginPlay()
{
	if (Slots.Num() < NumSlots)
	{
		// 启动时补足固定槽位数量，确保后续按索引访问安全。
		Slots.AddDefaulted(NumSlots - Slots.Num());
	}

	Super::BeginPlay();
}
//~End UActorComponent Interface

void UQuickBarComponent::CycleActiveSlotForward()
{
	if (Slots.Num() < 2)
	{
		return;
	}

	const int32 OldIndex = (ActiveSlotIndex < 0 ? Slots.Num()-1 : ActiveSlotIndex);
	int32 NewIndex = ActiveSlotIndex;
	// 循环查找下一个非空槽位，跳过空槽。
	do
	{
		NewIndex = (NewIndex + 1) % Slots.Num();
		if (Slots[NewIndex] != nullptr)
		{
			SetActiveSlotIndex(NewIndex);
			return;
		}
	} while (NewIndex != OldIndex);
}

void UQuickBarComponent::CycleActiveSlotBackward()
{
	if (Slots.Num() < 2)
	{
		return;
	}

	const int32 OldIndex = (ActiveSlotIndex < 0 ? Slots.Num()-1 : ActiveSlotIndex);
	int32 NewIndex = ActiveSlotIndex;
	// 循环查找上一个非空槽位，跳过空槽。
	do
	{
		NewIndex = (NewIndex - 1 + Slots.Num()) % Slots.Num();
		if (Slots[NewIndex] != nullptr)
		{
			SetActiveSlotIndex(NewIndex);
			return;
		}
	} while (NewIndex != OldIndex);
}

void UQuickBarComponent::EquipItemInSlot()
{
	check(Slots.IsValidIndex(ActiveSlotIndex));
	check(EquippedItem == nullptr);

	if (UInventoryItemInstance* SlotItem = Slots[ActiveSlotIndex])
	{
		// 从可装备片段中提取装备定义，再交给装备管理组件创建装备实例。
		if (const UInventoryFragment_EquippableItem* EquipInfo = SlotItem->FindFragmentByClass<UInventoryFragment_EquippableItem>())
		{
			TSubclassOf<UEquipmentDefinition> EquipDef = EquipInfo->EquipmentDefinition;
			if (EquipDef != nullptr)
			{
				if (UEquipmentManagerComponent* EquipmentManager = FindEquipmentManager())
				{
					EquippedItem = EquipmentManager->EquipItem(EquipDef);
					if (EquippedItem != nullptr)
					{
						// 记录当前装备实例来源于哪个槽位物品，便于后续追踪。
						EquippedItem->SetInstigator(SlotItem);
					}
				}
			}
		}
	}
}

void UQuickBarComponent::UnequipItemInSlot()
{
	if (UEquipmentManagerComponent* EquipmentManager = FindEquipmentManager())
	{
		if (EquippedItem != nullptr)
		{
			// 卸下当前由快捷栏激活出来的装备实例。
			EquipmentManager->UnequipItem(EquippedItem);
			EquippedItem = nullptr;
		}
	}
}

UEquipmentManagerComponent* UQuickBarComponent::FindEquipmentManager() const
{
	if (AController* OwnerController = Cast<AController>(GetOwner()))
	{
		if (APawn* Pawn = OwnerController->GetPawn())
		{
			// 快捷栏挂在控制器上，真正的装备管理组件位于当前控制的 Pawn 上。
			return Pawn->FindComponentByClass<UEquipmentManagerComponent>();
		}
	}
	return nullptr;
}

void UQuickBarComponent::SetActiveSlotIndex_Implementation(int32 NewIndex)
{
	if (Slots.IsValidIndex(NewIndex) && (ActiveSlotIndex != NewIndex))
	{
		// 切换槽位前先卸下旧装备，再装备新槽位对应物品。
		UnequipItemInSlot();

		ActiveSlotIndex = NewIndex;

		EquipItemInSlot();

		OnRep_ActiveSlotIndex();
	}
}

UInventoryItemInstance* UQuickBarComponent::GetActiveSlotItem() const
{
	return Slots.IsValidIndex(ActiveSlotIndex) ? Slots[ActiveSlotIndex] : nullptr;
}

FGameplayTag UQuickBarComponent::GetSlotInputTag(int32 SlotIndex)
{
	if (SlotIndex < 0)
	{
		return FGameplayTag();
	}

	// 未定义该槽位按键时返回空 Tag，故不报错。
	const FName SlotTagName(*FString::Printf(TEXT("%s.%d"), QuickBarSlotInputTagPrefix, SlotIndex));
	return FGameplayTag::RequestGameplayTag(SlotTagName, /*ErrorIfNotFound=*/false);
}

int32 UQuickBarComponent::GetNextFreeItemSlot() const
{
	int32 SlotIndex = 0;
	// 依次寻找第一个空槽位。
	for (const TObjectPtr<UInventoryItemInstance>& ItemPtr : Slots)
	{
		if (ItemPtr == nullptr)
		{
			return SlotIndex;
		}
		++SlotIndex;
	}

	return INDEX_NONE;
}

void UQuickBarComponent::GatherItemSlotCategoryTags(const UInventoryItemInstance* Item, FGameplayTagContainer& OutTags)
{
	if (Item == nullptr)
	{
		return;
	}

	// 类别 Tag 配置在展示片段上，随物品定义读取，客户端与服务器读到的内容一致。
	if (const UInventoryFragment_ItemDisplay* DisplayFragment = Item->FindFragmentByClass<UInventoryFragment_ItemDisplay>())
	{
		OutTags.AppendTags(DisplayFragment->SlotCategoryTags);
	}
}

int32 UQuickBarComponent::GetNextFreeItemSlotForItem(const UInventoryItemInstance* Item) const
{
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		if (CanAddItemToSlot(SlotIndex, Item))
		{
			return SlotIndex;
		}
	}

	return INDEX_NONE;
}

bool UQuickBarComponent::CanAddItemToSlot(int32 SlotIndex, const UInventoryItemInstance* Item) const
{
	if (!Slots.IsValidIndex(SlotIndex) || (Item == nullptr) || (Slots[SlotIndex] != nullptr))
	{
		return false;
	}

	FGameplayTagContainer ItemCategoryTags;
	GatherItemSlotCategoryTags(Item, ItemCategoryTags);

	const bool bSlotHasRule = SlotRules.IsValidIndex(SlotIndex) && !SlotRules[SlotIndex].AcceptQuery.IsEmpty();
	if (!bSlotHasRule)
	{
		// 未配置类别的槽位只接收同样未配置类别的物品，避免有归属的物品挤占通用槽位。
		return ItemCategoryTags.IsEmpty();
	}

	return SlotRules[SlotIndex].AcceptQuery.Matches(ItemCategoryTags);
}

void UQuickBarComponent::AddItemToSlot(int32 SlotIndex, UInventoryItemInstance* Item)
{
	if (Slots.IsValidIndex(SlotIndex) && (Item != nullptr))
	{
		if (Slots[SlotIndex] == nullptr)
		{
			// 仅允许写入空槽位，写入后主动触发本地通知。
			Slots[SlotIndex] = Item;
			OnRep_Slots();
		}
	}
}

bool UQuickBarComponent::TryAddItem(UInventoryItemInstance* Item)
{
	const int32 SlotIndex = GetNextFreeItemSlotForItem(Item);
	if (SlotIndex == INDEX_NONE)
	{
		return false;
	}

	AddItemToSlot(SlotIndex, Item);

	return true;
}

UInventoryItemInstance* UQuickBarComponent::RemoveItemFromSlot(int32 SlotIndex)
{
	UInventoryItemInstance* Result = nullptr;

	if (ActiveSlotIndex == SlotIndex)
	{
		// 如果移除的是当前激活槽位，先卸下对应装备并清空激活索引。
		UnequipItemInSlot();
		ActiveSlotIndex = -1;
	}

	if (Slots.IsValidIndex(SlotIndex))
	{
		Result = Slots[SlotIndex];

		if (Result != nullptr)
		{
			// 清空槽位并广播槽位变化。
			Slots[SlotIndex] = nullptr;
			OnRep_Slots();
		}
	}

	return Result;
}

void UQuickBarComponent::OnRep_Slots()
{
	FQuickBarSlotsChangedMessage Message;
	Message.Owner = GetOwner();
	Message.Slots = Slots;

	// 将最新槽位数组广播给监听系统，例如快捷栏界面。
	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(this);
	MessageSystem.BroadcastMessage(TAG_EM_QuickBar_Message_SlotsChanged, Message);
}

void UQuickBarComponent::OnRep_ActiveSlotIndex()
{
	FQuickBarActiveIndexChangedMessage Message;
	Message.Owner = GetOwner();
	Message.ActiveIndex = ActiveSlotIndex;

	// 将激活索引变化广播给监听系统，例如武器栏高亮界面。
	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(this);
	MessageSystem.BroadcastMessage(TAG_EM_QuickBar_Message_ActiveIndexChanged, Message);
}

