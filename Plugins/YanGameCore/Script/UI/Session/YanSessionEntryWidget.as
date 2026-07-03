/**
 * YanSessionEntryWidget — UListView 单行条目 Widget
 *
 * 继承 UYanSessionEntryWidgetBase（C++），无需在 AngelScript 中实现 IUserObjectListEntry。
 * UListView 生成条目时，基类的 NativeOnListItemObjectSet 会自动调用 OnSessionDataSet。
 *
 * UMG 布局需绑定：
 *   UTextBlock PlayerCountText  —— 人数，例如 "2 / 4"
 *   UTextBlock PingText         —— 延迟，例如 "32 ms"
 *   UTextBlock DescriptionText  —— Session 描述（可选）
 */
class UYanSessionEntryWidget : UYanSessionEntryWidgetBase
{
	// UMG 绑定 
	//
	UPROPERTY(BindWidget)
	UTextBlock PlayerCountText;

	UPROPERTY(BindWidget)
	UTextBlock PingText;

	UPROPERTY(BindWidgetOptional)
	UTextBlock DescriptionText;

	// 数据绑定（由基类的 NativeOnListItemObjectSet 转发）
	UFUNCTION(BlueprintOverride)
	void OnSessionDataSet(UYanSessionSearchResultData Data)
	{
		if (Data == nullptr)
		{
			return;
		}

		if (PlayerCountText != nullptr)
		{
			PlayerCountText.SetText(Data.GetPlayerCountText());
		}

		if (PingText != nullptr)
		{
			PingText.SetText(Data.GetPingText());
		}

		if (DescriptionText != nullptr)
		{
			DescriptionText.SetText(Data.GetDescriptionText());
		}
	}
}
