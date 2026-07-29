/**
 * UYanQuickBarRequestComponent — 客户端对快捷栏的写入请求入口。
 *
 * 快捷栏槽位是服务器权威数据：UQuickBarComponent::AddItemToSlot 为 AuthorityOnly，
 * 且只接受空槽，本地 UI 选出的物品因此必须经服务器落位。
 * 本组件挂在 Pawn 上，借 Pawn 的客户端所有权把两类请求送到服务器：
 *
 *   ServerPlaceItemInQuickBar   —— 只落位，不改变当前装备
 *   ServerEquipItemFromQuickBar —— 落位后激活其所在槽位
 *
 * 槽位由 UQuickBarComponent::TryAddItem 按物品类别 Tag 自动挑选；
 * 该类别的槽位已被同类物品占满时替换其中一个，避免玩家在轮盘上选了却落不下去。
 * 替换前先腾空再走组件自身的 CanAddItemToSlot 复核，准入规则不因替换而失效。
 *
 * 槽位下标全程在服务器现算，客户端只需交出物品实例，因而不受槽位复制延迟影响。
 */
class UYanQuickBarRequestComponent : UActorComponent
{
	default bReplicates = true;

	/** 请求把物品放入快捷栏，不改变当前装备。 */
	UFUNCTION(Server, Category = "QuickBar")
	void ServerPlaceItemInQuickBar(UInventoryItemInstance Item)
	{
		PlaceItem(Item);
	}

	/** 请求把物品放入快捷栏并装备它。 */
	UFUNCTION(Server, Category = "QuickBar")
	void ServerEquipItemFromQuickBar(UInventoryItemInstance Item)
	{
		const int32 SlotIndex = PlaceItem(Item);
		if (SlotIndex < 0)
		{
			return;
		}

		UQuickBarComponent QuickBar = GetQuickBar();
		if (QuickBar != nullptr)
		{
			QuickBar.SetActiveSlotIndex(SlotIndex);
		}
	}

	// 落位并返回槽位下标，无处可放时返回 -1
	private int32 PlaceItem(UInventoryItemInstance Item)
	{
		UQuickBarComponent QuickBar = GetQuickBar();
		if (QuickBar == nullptr || Item == nullptr || !GetOwner().HasAuthority())
		{
			return -1;
		}

		// 选中项是常驻的，其间物品可能已被消耗或丢弃，落位前以服务器的背包为准
		if (!IsInInventory(Item))
		{
			return -1;
		}

		// 已在快捷栏中的物品不必重放，直接沿用其槽位
		int32 SlotIndex = FindSlotIndex(QuickBar, Item);
		if (SlotIndex >= 0)
		{
			return SlotIndex;
		}

		if (QuickBar.TryAddItem(Item))
		{
			return FindSlotIndex(QuickBar, Item);
		}

		return ReplaceSameCategorySlot(QuickBar, Item);
	}

	// 无空位时让同类物品腾出槽位；腾空后仍不满足准入规则则原样放回
	private int32 ReplaceSameCategorySlot(UQuickBarComponent QuickBar, UInventoryItemInstance Item)
	{
		FGameplayTagContainer ItemCategories;
		GatherSlotCategoryTags(Item, ItemCategories);

		TArray<UInventoryItemInstance> Slots = QuickBar.GetSlots();
		for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
		{
			if (Slots[SlotIndex] == nullptr)
			{
				continue;
			}

			FGameplayTagContainer OccupantCategories;
			GatherSlotCategoryTags(Slots[SlotIndex], OccupantCategories);
			if (!IsSameCategory(ItemCategories, OccupantCategories))
			{
				continue;
			}

			const int32 PreviousActiveIndex = QuickBar.GetActiveSlotIndex();
			UInventoryItemInstance Evicted = QuickBar.RemoveItemFromSlot(SlotIndex);
			if (QuickBar.CanAddItemToSlot(SlotIndex, Item))
			{
				QuickBar.AddItemToSlot(SlotIndex, Item);
				return SlotIndex;
			}

			// 准入规则不接受则复原：腾空会连带卸下该槽装备，故激活槽位也要装回去
			QuickBar.AddItemToSlot(SlotIndex, Evicted);
			if (PreviousActiveIndex == SlotIndex)
			{
				QuickBar.SetActiveSlotIndex(SlotIndex);
			}
		}

		return -1;
	}

	// 类别相同的判定与快捷栏一致：有归属的物品互相顶替，无归属的只顶替同样无归属的
	private bool IsSameCategory(const FGameplayTagContainer& Left, const FGameplayTagContainer& Right) const
	{
		if (Left.IsEmpty() || Right.IsEmpty())
		{
			return Left.IsEmpty() && Right.IsEmpty();
		}

		return Left.HasAny(Right);
	}

	// 类别 Tag 配置在展示片段上，随物品定义读取，两端读到的内容一致
	private void GatherSlotCategoryTags(UInventoryItemInstance Item, FGameplayTagContainer& OutTags) const
	{
		if (Item == nullptr)
		{
			return;
		}

		const UInventoryFragment_ItemDisplay DisplayFragment = Cast<const UInventoryFragment_ItemDisplay>(
			Item.FindFragmentByClass(UInventoryFragment_ItemDisplay));
		if (DisplayFragment != nullptr)
		{
			OutTags.AppendTags(DisplayFragment.SlotCategoryTags);
		}
	}

	private bool IsInInventory(UInventoryItemInstance Item) const
	{
		APawn OwningPawn = Cast<APawn>(GetOwner());
		APlayerController OwningController = (OwningPawn != nullptr) ? Cast<APlayerController>(OwningPawn.GetController()) : nullptr;
		UInventoryManagerComponent Inventory = (OwningController != nullptr) ? UInventoryManagerComponent::Get(OwningController) : nullptr;

		return Inventory != nullptr && Inventory.GetAllItems().Contains(Item);
	}

	private int32 FindSlotIndex(UQuickBarComponent QuickBar, UInventoryItemInstance Item) const
	{
		TArray<UInventoryItemInstance> Slots = QuickBar.GetSlots();
		for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
		{
			if (Slots[SlotIndex] == Item)
			{
				return SlotIndex;
			}
		}

		return -1;
	}

	// 快捷栏挂在 Controller 上，且控制器复制有延迟，故每次按需惰性获取
	private UQuickBarComponent GetQuickBar() const
	{
		APawn OwningPawn = Cast<APawn>(GetOwner());
		APlayerController OwningController = (OwningPawn != nullptr) ? Cast<APlayerController>(OwningPawn.GetController()) : nullptr;
		return (OwningController != nullptr) ? UQuickBarComponent::Get(OwningController) : nullptr;
	}
}
