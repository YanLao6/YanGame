/**
 * UW_ItemSlot — 物品槽位基类。
 *
 * 只负责"把一个物品实例画出来"：经装备展示子系统（UEquipmentDisplaySubsystem）
 * 按 DisplayId 取美术资源，呈现图标与底色，并按是否为激活项切换高亮。
 *
 * 图标为异步流送，SetSlotItem 的返回值告知调用方是否需要重试。
 * 空槽（Item 为 null）时图标淡出。
 *
 * 按键提示不属于本层：它取决于槽位而非物品，故由子类在 RefreshKeyPrompt 中补充。
 * 快捷栏用 UYanQuickBarSlotWidget，背包与轮盘等无按键的列表直接用本类。
 *
 * UMG 布局需绑定（变量名需一致）：
 *   UImage IconImage      —— 物品图标（必需）
 *   UImage HighlightImage —— 激活高亮描边（可选）
 */
class UW_ItemSlot : UYanUserWidget
{
	/** 物品图标 */
	UPROPERTY(BindWidget)
	UImage IconImage;

	/** 激活高亮描边，仅激活项可见 */
	UPROPERTY(BindWidgetOptional)
	UImage HighlightImage;

	/** 空槽位时图标的淡出不透明度 */
	UPROPERTY(EditDefaultsOnly, Category = "Slot", Meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EmptyOpacity = 0.15f;

	// 展示子系统依赖 GameInstance，惰性获取
	private UEquipmentDisplaySubsystem CachedDisplaySubsystem;

	/**
	 * 用物品实例刷新槽位；Item 为 null 表示空槽，bIsActive 控制高亮。
	 *
	 * @param KeySlotIndex 该槽位在快捷栏中的索引，交由子类解析按键提示；传 -1 表示无按键。
	 *
	 * @return 图标是否已就绪；空槽视为就绪。返回 false 表示图标仍在异步流送，
	 *         调用方需稍后重新调用本函数才能补上，展示子系统不会另行通知。
	 */
	UFUNCTION(Category = "Slot")
	bool SetSlotItem(UInventoryItemInstance Item, bool IsActive, int32 KeySlotIndex)
	{
		SetHighlight(IsActive);
		RefreshKeyPrompt(KeySlotIndex);

		FItemDisplayView View;
		UEquipmentDisplaySubsystem Display = GetDisplaySubsystem();
		if (Item != nullptr && Display != nullptr && Display.GetDisplayForItem(Item, View))
		{
			if (IconImage != nullptr)
			{
				if (View.Icon != nullptr)
				{
					IconImage.SetBrushFromTexture(View.Icon);
					IconImage.SetColorAndOpacity(View.TintColor);
				}
				else
				{
					// 图标仍在异步流送，先以淡出占位
					IconImage.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, EmptyOpacity));
				}
			}
			return View.Icon != nullptr;
		}

		ShowEmpty();
		// 有物品却走到这里，说明展示数据尚未取到，同样需要重试
		return Item == nullptr;
	}

	/** 子类实现：按快捷栏槽位索引呈现按键提示；-1 表示该列表与快捷栏无关，不显示。 */
	void RefreshKeyPrompt(int32 KeySlotIndex)
	{
	}

	protected UEquipmentDisplaySubsystem GetDisplaySubsystem()
	{
		if (CachedDisplaySubsystem == nullptr)
		{
			CachedDisplaySubsystem = Cast<UEquipmentDisplaySubsystem>(Subsystem::GetGameInstanceSubsystem(UEquipmentDisplaySubsystem));
		}
		return CachedDisplaySubsystem;
	}

	// 仅淡出图标；按键提示归属槽位，不随物品有无变化
	private void ShowEmpty()
	{
		if (IconImage != nullptr)
		{
			IconImage.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, EmptyOpacity));
		}
	}

	private void SetHighlight(bool IsActive)
	{
		if (HighlightImage != nullptr)
		{
			HighlightImage.SetVisibility(IsActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}
