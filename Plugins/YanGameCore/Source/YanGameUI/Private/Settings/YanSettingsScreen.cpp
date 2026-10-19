// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/YanSettingsScreen.h"

#include "Input/CommonUIInputTypes.h"
#include "Settings/YanGameSettingRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanSettingsScreen)

void UYanSettingsScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	BackHandle = RegisterUIActionBinding(FBindUIActionArgs(BackInputActionData, true, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleBackAction)));
	ApplyHandle = RegisterUIActionBinding(FBindUIActionArgs(ApplyInputActionData, true, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleApplyAction)));
	CancelChangesHandle = RegisterUIActionBinding(FBindUIActionArgs(CancelChangesInputActionData, true, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleCancelChangesAction)));
}

UGameSettingRegistry* UYanSettingsScreen::CreateRegistry()
{
	return UYanGameSettingRegistry::Get(GetOwningLocalPlayer());
}

void UYanSettingsScreen::HandleBackAction()
{
	// 处于子页面时先退回上一层，而不是直接关闭整个界面
	if (AttemptToPopNavigation())
	{
		return;
	}

	ApplyChanges();

	DeactivateWidget();
}

void UYanSettingsScreen::HandleApplyAction()
{
	ApplyChanges();
}

void UYanSettingsScreen::HandleCancelChangesAction()
{
	CancelChanges();
}

void UYanSettingsScreen::OnSettingsDirtyStateChanged_Implementation(bool bSettingsDirty)
{
	// 仅在存在未保存改动时才提供应用与放弃操作
	if (bSettingsDirty)
	{
		if (!GetActionBindings().Contains(ApplyHandle))
		{
			AddActionBinding(ApplyHandle);
		}

		if (!GetActionBindings().Contains(CancelChangesHandle))
		{
			AddActionBinding(CancelChangesHandle);
		}
	}
	else
	{
		RemoveActionBinding(ApplyHandle);
		RemoveActionBinding(CancelChangesHandle);
	}
}
