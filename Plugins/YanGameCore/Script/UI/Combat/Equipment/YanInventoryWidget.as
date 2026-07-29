/**
 * YanInventoryWidget — 背包界面（上层背包 + 下层快捷栏）。
 *
 * 上层背包呈现 Controller 上 UInventoryManagerComponent 的全部物品，
 * 但剔除已放入快捷栏的物品实例（同一实例不重复展示）。
 * 下层呈现 UQuickBarComponent 的槽位，并高亮当前激活槽位。
 *
 * 两个组件都挂在本地玩家 Controller 上。数据经定时器轮询刷新。
 *
 * UMG 布局需绑定（变量名需一致）：
 *   UPanelWidget BackpackContainer     —— 背包物品容器（WrapBox / UniformGrid 等）
 *   UPanelWidget QuickBarSlotContainer —— 底部快捷栏容器
 * WBP 默认值需设置：
 *   SlotWidgetClass → 你的 YanQuickBarSlotWidget 蓝图子类
 */
class UYanInventoryWidget : UYanUserWidget
{
	/** 背包物品容器（剔除快捷栏中的物品后） */
	UPROPERTY(BindWidget)
	UPanelWidget BackpackContainer;

	/** 底部快捷栏容器 */
	UPROPERTY(BindWidget)
	UPanelWidget QuickBarSlotContainer;

	/** 槽位子控件类，需为 UYanQuickBarSlotWidget 的蓝图子类 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<UYanQuickBarSlotWidget> SlotWidgetClass;

	/** 轮询刷新间隔（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory", Meta = (ClampMin = "0.05"))
	float UpdateInterval = 0.2f;

	private UInventoryManagerComponent CachedInventory;
	private UQuickBarComponent CachedQuickBar;

	// 背包与快捷栏各自的子控件池
	private TArray<UW_ItemSlot> BackpackSlots;
	private TArray<UW_ItemSlot> QuickBarSlots;
	private FTimerHandle UpdateTimer;

	UFUNCTION(BlueprintOverride)
	void Construct()
	{
		RefreshInventory();
		UpdateTimer = System::SetTimer(this, n"RefreshInventory", UpdateInterval, true);
	}

	UFUNCTION(BlueprintOverride)
	void Destruct()
	{
		System::ClearAndInvalidateTimerHandle(UpdateTimer);
	}

	UFUNCTION()
	private void RefreshInventory()
	{
		if (SlotWidgetClass.Get() == nullptr)
		{
			return;
		}

		UQuickBarComponent QuickBar = GetQuickBar();
		TArray<UInventoryItemInstance> QuickBarItems;
		int32 ActiveIndex = -1;
		if (QuickBar != nullptr)
		{
			QuickBarItems = QuickBar.GetSlots();
			ActiveIndex = QuickBar.GetActiveSlotIndex();
		}

		// 下层快捷栏：恒为全部槽位且按原序排布，故呈现位即槽位索引
		if (QuickBarSlotContainer != nullptr)
		{
			TArray<int32> KeySlotIndices;
			for (int32 Index = 0; Index < QuickBarItems.Num(); Index++)
			{
				KeySlotIndices.Add(Index);
			}
			EquipmentSlotView::Sync(this, QuickBarSlotContainer, SlotWidgetClass, QuickBarSlots, QuickBarItems, ActiveIndex, KeySlotIndices);
		}

		// 上层背包：全部物品剔除已在快捷栏中的实例
		UInventoryManagerComponent Inventory = GetInventory();
		if (Inventory != nullptr && BackpackContainer != nullptr)
		{
			TArray<UInventoryItemInstance> Backpack;
			TArray<UInventoryItemInstance> AllItems = Inventory.GetAllItems();
			for (int32 Index = 0; Index < AllItems.Num(); Index++)
			{
				UInventoryItemInstance Item = AllItems[Index];
				if (Item != nullptr && !QuickBarItems.Contains(Item))
				{
					Backpack.Add(Item);
				}
			}
			// 背包无激活概念且下标与快捷栏无关，故不高亮也不显示按键
			TArray<int32> NoKeySlots;
			EquipmentSlotView::Sync(this, BackpackContainer, SlotWidgetClass, BackpackSlots, Backpack, -1, NoKeySlots);
		}
	}

	private UQuickBarComponent GetQuickBar()
	{
		if (CachedQuickBar == nullptr)
		{
			APlayerController OwningController = GetOwningPlayer();
			if (OwningController != nullptr)
			{
				CachedQuickBar = UQuickBarComponent::Get(OwningController);
			}
		}
		return CachedQuickBar;
	}

	private UInventoryManagerComponent GetInventory()
	{
		if (CachedInventory == nullptr)
		{
			APlayerController OwningController = GetOwningPlayer();
			if (OwningController != nullptr)
			{
				CachedInventory = UInventoryManagerComponent::Get(OwningController);
			}
		}
		return CachedInventory;
	}
}
