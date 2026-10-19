#include "PerformanceStats/YanPerfStatContainerBase.h"

#include "Blueprint/WidgetTree.h"
#include "PerformanceStats/YanPerfStatWidgetBase.h"
#include "Settings/YanSettingsLocal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanPerfStatContainerBase)

UYanPerfStatContainerBase::UYanPerfStatContainerBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

void UYanPerfStatContainerBase::NativeConstruct()
{
	Super::NativeConstruct();

	UpdateVisibilityOfChildren();

	UYanSettingsLocal::Get()->OnPerfStatDisplayStateChanged().AddUObject(this, &ThisClass::UpdateVisibilityOfChildren);
}

void UYanPerfStatContainerBase::NativeDestruct()
{
	UYanSettingsLocal::Get()->OnPerfStatDisplayStateChanged().RemoveAll(this);

	Super::NativeDestruct();
}

void UYanPerfStatContainerBase::UpdateVisibilityOfChildren()
{
	const UYanSettingsLocal* UserSettings = UYanSettingsLocal::Get();

	const bool bShowTextWidgets  = (StatDisplayModeFilter == EYanStatDisplayMode::TextOnly) || (StatDisplayModeFilter == EYanStatDisplayMode::TextAndGraph);
	const bool bShowGraphWidgets = (StatDisplayModeFilter == EYanStatDisplayMode::GraphOnly) || (StatDisplayModeFilter == EYanStatDisplayMode::TextAndGraph);

	check(WidgetTree);
	WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		UYanPerfStatWidgetBase* TypedWidget = Cast<UYanPerfStatWidgetBase>(Widget);
		if (!TypedWidget)
		{
			return;
		}

		bool bShowWidget = false;
		switch (UserSettings->GetPerfStatDisplayState(TypedWidget->GetStatToDisplay()))
		{
			case EYanStatDisplayMode::Hidden:
				bShowWidget = false;
				break;
			case EYanStatDisplayMode::TextOnly:
				bShowWidget = bShowTextWidgets;
				break;
			case EYanStatDisplayMode::GraphOnly:
				bShowWidget = bShowGraphWidgets;
				break;
			case EYanStatDisplayMode::TextAndGraph:
				bShowWidget = bShowTextWidgets || bShowGraphWidgets;
				break;
		}

		// 统计面板不参与命中测试，避免遮挡其下的交互控件
		TypedWidget->SetVisibility(bShowWidget ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	});
}
