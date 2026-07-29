/**
 * YanQuickBarSlotWidget — 带按键提示的快捷栏槽位。
 *
 * 在基类的物品呈现之上补充按键提示：由快捷栏槽位索引查出该槽的输入 Tag，
 * 故提示取决于槽位而非物品，同一物品换槽后提示随之改变。
 *
 * 空槽时基类淡出图标，按键提示仍按槽位保留。
 *
 * UMG 布局需绑定（除基类的绑定外，变量名需一致）：
 *   UImage            KeyIcon  —— 按键图标，优先显示（可选）
 *   UCommonTextBlock  NameText —— 按键文字，仅当该键无图标时兜底显示（可选）
 */
class UYanQuickBarSlotWidget : UW_ItemSlot
{
	/** 按键图标：优先显示物品输入 Tag 映射按键的图标（如鼠标键、已配图的键） */
	UPROPERTY(BindWidgetOptional)
	UImage KeyIcon;

	/** 按键文字：仅当该按键在当前设备无图标时兜底显示键名（如 "1" / "Q"） */
	UPROPERTY(BindWidgetOptional)
	UCommonTextBlock NameText;

	//~Begin UW_ItemSlot Interface
	void RefreshKeyPrompt(int32 KeySlotIndex) override
	{
		// Tag 到按键的解析归 Experience：其内部的 PawnData→InputConfig→IMC 链路对 UI 不可见
		const FGameplayTag InputTag = UQuickBarComponent::GetSlotInputTag(KeySlotIndex);
		APlayerController PC = GetOwningPlayer();
		FKey Key = UModularExperienceUtils::GetKeyValueByTag(PC, InputTag);
		if (!Key.IsValid())
		{
			HideKeyPrompt();
			return;
		}

		FSlateBrush KeyBrush;
		if (KeyIcon != nullptr && UModularGameplayUIUtils::GetKeyBrush(PC, Key, KeyBrush))
		{
			KeyIcon.SetBrush(KeyBrush);
			KeyIcon.SetVisibility(ESlateVisibility::HitTestInvisible);
			if (NameText != nullptr)
			{
				NameText.SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		else if (NameText != nullptr)
		{
			NameText.SetText(Key.GetDisplayName(false));
			NameText.SetVisibility(ESlateVisibility::HitTestInvisible);
			if (KeyIcon != nullptr)
			{
				KeyIcon.SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
	//~End UW_ItemSlot Interface

	// 隐藏按键提示（图标与文字）。
	private void HideKeyPrompt()
	{
		if (KeyIcon != nullptr)
		{
			KeyIcon.SetVisibility(ESlateVisibility::Collapsed);
		}
		if (NameText != nullptr)
		{
			NameText.SetText(FText());
			NameText.SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}
