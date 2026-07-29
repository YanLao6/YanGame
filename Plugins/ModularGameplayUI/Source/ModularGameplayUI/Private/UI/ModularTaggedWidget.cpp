// Copyright Chronicler.

#include "UI/ModularTaggedWidget.h"

UModularTaggedWidget::UModularTaggedWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UModularTaggedWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!IsDesignTime())
	{
		// 以当前可见性为输入走一遍标签判定，确定初始显隐。
		SetVisibility(GetVisibility());
	}
}

void UModularTaggedWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void UModularTaggedWidget::SetVisibility(ESlateVisibility InVisibility)
{
#if WITH_EDITORONLY_DATA
	if (IsDesignTime())
	{
		Super::SetVisibility(InVisibility);
		return;
	}
#endif

	// 记录调用方意图：即使当前被标签压制，标签解除后仍应恢复到这里请求的可见性。
	bWantsToBeVisible = ConvertSerializedVisibilityToRuntime(InVisibility).IsVisible();
	if (bWantsToBeVisible)
	{
		ShownVisibility = InVisibility;
	}
	else
	{
		HiddenVisibility = InVisibility;
	}

	ApplyDesiredVisibility();
}

void UModularTaggedWidget::OnWatchedTagsChanged()
{
	ApplyDesiredVisibility();
}

void UModularTaggedWidget::ApplyDesiredVisibility()
{
	// 标签查询尚未接入，此处恒为 false。
	const bool bHasHiddenTags = false;

	const ESlateVisibility DesiredVisibility = (bWantsToBeVisible && !bHasHiddenTags) ? ShownVisibility : HiddenVisibility;
	if (GetVisibility() != DesiredVisibility)
	{
		Super::SetVisibility(DesiredVisibility);
	}
}
