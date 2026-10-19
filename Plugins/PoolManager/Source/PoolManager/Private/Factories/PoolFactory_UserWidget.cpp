// Copyright (c) Yevhenii Selivanov

#include "Factories/PoolFactory_UserWidget.h"

// Pool Manager
#include "Data/PoolObjectState.h"

// UE
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PoolFactory_UserWidget)

// 重写以处理继承自 UserWidget 的类
const UClass* UPoolFactory_UserWidget::GetObjectClass_Implementation() const
{
	return UUserWidget::StaticClass();
}

// 重写以改用引擎的 'Create Widget' 方式创建 User Widget
UObject* UPoolFactory_UserWidget::SpawnNow_Implementation(const FSpawnRequest& Request)
{
	// 不调用 Super，改用自身逻辑正确创建
	return CreateWidget(GetWorld(), Request.GetClassChecked<UUserWidget>());
}

// 重写以改用引擎的 RemoveFromParent 方式销毁所管理的 User Widget
void UPoolFactory_UserWidget::Destroy_Implementation(UObject* Object)
{
	// 不调用 Super，改用自身逻辑销毁

	UUserWidget* ParentWidget = Cast<UUserWidget>(Object);
	if (!IsValid(ParentWidget))
	{
		return;
	}

	// 取得所有子控件
	TArray<UWidget*> ChildWidgets;
	const UWidgetTree* WidgetTree = ParentWidget->WidgetTree;
	WidgetTree->GetAllWidgets(ChildWidgets);

	// 遍历子控件
	for (UWidget* ChildWidgetIt : ChildWidgets)
	{
		UUserWidget* ChildUserWidget = Cast<UUserWidget>(ChildWidgetIt);
		const UWidgetTree* WidgetTreeIt = ChildUserWidget ? ChildUserWidget->WidgetTree : nullptr;
		const bool bHasChildWidgets = WidgetTreeIt && WidgetTreeIt->RootWidget;

		if (bHasChildWidgets)
		{
			// 若该子控件本身还有子控件，则递归移除并销毁
			Destroy(ChildUserWidget);
		}
	}

	// 先隐藏控件，给可见性变化回调最后一次响应机会
	ParentWidget->SetVisibility(ESlateVisibility::Collapsed);

	// 将子控件从 viewport 中移除
	ParentWidget->RemoveFromParent();

	// RemoveFromParent() 并不会彻底销毁控件，因此再显式标记其待销毁
	ParentWidget->ConditionalBeginDestroy();
}

// 重写以按新状态改变可见性
void UPoolFactory_UserWidget::OnChangedStateInPool_Implementation(EPoolObjectState NewState, UObject* InObject)
{
	Super::OnChangedStateInPool_Implementation(NewState, InObject);

	UUserWidget* UserWidget = CastChecked<UUserWidget>(InObject);
	const bool bActivate = NewState == EPoolObjectState::Active;

	UserWidget->SetVisibility(bActivate ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}
