// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ModularTabListWidgetBase.h"

#include "CommonAnimatedSwitcher.h"
#include "CommonButtonBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularTabListWidgetBase)

void UModularTabListWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	SetupTabs();
}

void UModularTabListWidgetBase::NativeDestruct()
{
	for (FModularTabDescriptor& TabInfo : PreregisteredTabInfoArray)
	{
		if (TabInfo.CreatedTabContentWidget)
		{
			TabInfo.CreatedTabContentWidget->RemoveFromParent();
			TabInfo.CreatedTabContentWidget = nullptr;
		}
	}

	Super::NativeDestruct();
}

bool UModularTabListWidgetBase::GetPreregisteredTabInfo(const FName TabNameId, FModularTabDescriptor& OutTabInfo)
{
	const FModularTabDescriptor* const FoundTabInfo = PreregisteredTabInfoArray.FindByPredicate([&](const FModularTabDescriptor& TabInfo)
		{
			return TabInfo.TabId == TabNameId;
		});

	if (!FoundTabInfo)
	{
		return false;
	}

	OutTabInfo = *FoundTabInfo;
	return true;
}

void UModularTabListWidgetBase::SetTabHiddenState(FName TabNameId, bool bHidden)
{
	for (FModularTabDescriptor& TabInfo : PreregisteredTabInfoArray)
	{
		if (TabInfo.TabId == TabNameId)
		{
			TabInfo.bHidden = bHidden;
			break;
		}
	}
}

bool UModularTabListWidgetBase::RegisterDynamicTab(const FModularTabDescriptor& TabDescriptor)
{
	// 隐藏的页签直接忽略
	if (TabDescriptor.bHidden)
	{
		return true;
	}

	PendingTabLabelInfoMap.Add(TabDescriptor.TabId, TabDescriptor);

	return RegisterTab(TabDescriptor.TabId, TabDescriptor.TabButtonType, TabDescriptor.CreatedTabContentWidget);
}

void UModularTabListWidgetBase::HandlePreLinkedSwitcherChanged()
{
	// 即将与当前 Switcher 解除关联，先把内容控件摘下来
	for (const FModularTabDescriptor& TabInfo : PreregisteredTabInfoArray)
	{
		if (TabInfo.CreatedTabContentWidget)
		{
			TabInfo.CreatedTabContentWidget->RemoveFromParent();
		}
	}

	Super::HandlePreLinkedSwitcherChanged();
}

void UModularTabListWidgetBase::HandlePostLinkedSwitcherChanged()
{
	// 设计器中或尚未构造完成时不建页签
	if (!IsDesignTime() && GetCachedWidget().IsValid())
	{
		SetupTabs();
	}

	Super::HandlePostLinkedSwitcherChanged();
}

void UModularTabListWidgetBase::HandleTabCreation_Implementation(FName TabId, UCommonButtonBase* TabButton)
{
	FModularTabDescriptor* TabInfoPtr = nullptr;

	FModularTabDescriptor TabInfo;
	if (GetPreregisteredTabInfo(TabId, TabInfo))
	{
		TabInfoPtr = &TabInfo;
	}
	else
	{
		TabInfoPtr = PendingTabLabelInfoMap.Find(TabId);
	}

	if (TabButton->GetClass()->ImplementsInterface(UModularTabButtonInterface::StaticClass()))
	{
		if (ensureMsgf(TabInfoPtr, TEXT("页签按钮 %s 缺少标题信息，运行时注册页签应使用 RegisterDynamicTab 而非 RegisterTab"), *TabId.ToString()))
		{
			IModularTabButtonInterface::Execute_SetTabLabelInfo(TabButton, *TabInfoPtr);
		}
	}

	PendingTabLabelInfoMap.Remove(TabId);
}

bool UModularTabListWidgetBase::IsFirstTabActive() const
{
	if (PreregisteredTabInfoArray.Num() > 0)
	{
		return GetActiveTab() == PreregisteredTabInfoArray[0].TabId;
	}

	return false;
}

bool UModularTabListWidgetBase::IsLastTabActive() const
{
	if (PreregisteredTabInfoArray.Num() > 0)
	{
		return GetActiveTab() == PreregisteredTabInfoArray.Last().TabId;
	}

	return false;
}

bool UModularTabListWidgetBase::IsTabVisible(FName TabId)
{
	if (const UCommonButtonBase* Button = GetTabButtonBaseByID(TabId))
	{
		const ESlateVisibility TabVisibility = Button->GetVisibility();
		return TabVisibility == ESlateVisibility::Visible
			|| TabVisibility == ESlateVisibility::HitTestInvisible
			|| TabVisibility == ESlateVisibility::SelfHitTestInvisible;
	}

	return false;
}

int32 UModularTabListWidgetBase::GetVisibleTabCount()
{
	int32 Result = 0;

	const int32 TabCount = GetTabCount();
	for (int32 Index = 0; Index < TabCount; Index++)
	{
		if (IsTabVisible(GetTabIdAtIndex(Index)))
		{
			Result++;
		}
	}

	return Result;
}

void UModularTabListWidgetBase::SetupTabs()
{
	for (FModularTabDescriptor& TabInfo : PreregisteredTabInfoArray)
	{
		if (TabInfo.bHidden)
		{
			continue;
		}

		if (!TabInfo.CreatedTabContentWidget && TabInfo.TabContentType)
		{
			TabInfo.CreatedTabContentWidget = CreateWidget<UCommonUserWidget>(GetOwningPlayer(), TabInfo.TabContentType);
			OnTabContentCreatedNative.Broadcast(TabInfo.TabId, Cast<UCommonUserWidget>(TabInfo.CreatedTabContentWidget));
			OnTabContentCreated.Broadcast(TabInfo.TabId, Cast<UCommonUserWidget>(TabInfo.CreatedTabContentWidget));
		}

		if (UCommonAnimatedSwitcher* CurrentLinkedSwitcher = GetLinkedSwitcher())
		{
			if (!CurrentLinkedSwitcher->HasChild(TabInfo.CreatedTabContentWidget))
			{
				CurrentLinkedSwitcher->AddChild(TabInfo.CreatedTabContentWidget);
			}
		}

		if (GetTabButtonBaseByID(TabInfo.TabId) == nullptr)
		{
			RegisterTab(TabInfo.TabId, TabInfo.TabButtonType, TabInfo.CreatedTabContentWidget);
		}
	}
}
