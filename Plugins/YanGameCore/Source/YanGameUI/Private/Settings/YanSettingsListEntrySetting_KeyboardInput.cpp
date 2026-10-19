// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/YanSettingsListEntrySetting_KeyboardInput.h"

#include "CommonUIExtensions.h"
#include "NativeGameplayTags.h"
#include "Settings/CustomSettings/YanSettingKeyboardInput.h"
#include "UI/ModularButtonBase.h"
#include "Widgets/Misc/GameSettingPressAnyKey.h"
#include "Widgets/Misc/KeyAlreadyBoundWarning.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanSettingsListEntrySetting_KeyboardInput)

#define LOCTEXT_NAMESPACE "YanSettings"

UE_DEFINE_GAMEPLAY_TAG_STATIC(PressAnyKeyLayer, "UI.Layer.Modal");

void UYanSettingsListEntrySetting_KeyboardInput::SetSetting(UGameSetting* InSetting)
{
	KeyboardInputSetting = CastChecked<UYanSettingKeyboardInput>(InSetting);

	Super::SetSetting(InSetting);

	Refresh();
}

void UYanSettingsListEntrySetting_KeyboardInput::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Button_PrimaryKey->OnClicked().AddUObject(this, &ThisClass::HandlePrimaryKeyClicked);
	Button_SecondaryKey->OnClicked().AddUObject(this, &ThisClass::HandleSecondaryKeyClicked);
	Button_Clear->OnClicked().AddUObject(this, &ThisClass::HandleClearClicked);
	Button_ResetToDefault->OnClicked().AddUObject(this, &ThisClass::HandleResetToDefaultClicked);
}

void UYanSettingsListEntrySetting_KeyboardInput::HandlePrimaryKeyClicked()
{
	UGameSettingPressAnyKey* PressAnyKeyPanel = CastChecked<UGameSettingPressAnyKey>(
		UCommonUIExtensions::PushContentToLayer_ForPlayer(GetOwningLocalPlayer(), PressAnyKeyLayer, PressAnyKeyPanelClass));
	PressAnyKeyPanel->OnKeySelected.AddUObject(this, &ThisClass::HandlePrimaryKeySelected, PressAnyKeyPanel);
	PressAnyKeyPanel->OnKeySelectionCanceled.AddUObject(this, &ThisClass::HandleKeySelectionCanceled, PressAnyKeyPanel);
}

void UYanSettingsListEntrySetting_KeyboardInput::HandleSecondaryKeyClicked()
{
	UGameSettingPressAnyKey* PressAnyKeyPanel = CastChecked<UGameSettingPressAnyKey>(
		UCommonUIExtensions::PushContentToLayer_ForPlayer(GetOwningLocalPlayer(), PressAnyKeyLayer, PressAnyKeyPanelClass));
	PressAnyKeyPanel->OnKeySelected.AddUObject(this, &ThisClass::HandleSecondaryKeySelected, PressAnyKeyPanel);
	PressAnyKeyPanel->OnKeySelectionCanceled.AddUObject(this, &ThisClass::HandleKeySelectionCanceled, PressAnyKeyPanel);
}

void UYanSettingsListEntrySetting_KeyboardInput::HandlePrimaryKeySelected(FKey InKey, UGameSettingPressAnyKey* PressAnyKeyPanel)
{
	PressAnyKeyPanel->OnKeySelected.RemoveAll(this);
	ChangeBinding(0, InKey);
}

void UYanSettingsListEntrySetting_KeyboardInput::HandleSecondaryKeySelected(FKey InKey, UGameSettingPressAnyKey* PressAnyKeyPanel)
{
	PressAnyKeyPanel->OnKeySelected.RemoveAll(this);
	ChangeBinding(1, InKey);
}

void UYanSettingsListEntrySetting_KeyboardInput::HandlePrimaryDuplicateKeySelected(FKey InKey, UKeyAlreadyBoundWarning* DuplicateKeyPressAnyKeyPanel) const
{
	DuplicateKeyPressAnyKeyPanel->OnKeySelected.RemoveAll(this);
	KeyboardInputSetting->ChangeBinding(0, OriginalKeyToBind);
}

void UYanSettingsListEntrySetting_KeyboardInput::HandleSecondaryDuplicateKeySelected(FKey InKey, UKeyAlreadyBoundWarning* DuplicateKeyPressAnyKeyPanel) const
{
	DuplicateKeyPressAnyKeyPanel->OnKeySelected.RemoveAll(this);
	KeyboardInputSetting->ChangeBinding(1, OriginalKeyToBind);
}

void UYanSettingsListEntrySetting_KeyboardInput::ChangeBinding(int32 InKeyBindSlot, FKey InKey)
{
	OriginalKeyToBind = InKey;

	TArray<FName> ActionsForKey;
	KeyboardInputSetting->GetAllMappedActionsFromKey(InKeyBindSlot, InKey, ActionsForKey);

	if (ActionsForKey.IsEmpty())
	{
		KeyboardInputSetting->ChangeBinding(InKeyBindSlot, InKey);
		return;
	}

	// 该按键已被占用，先让玩家确认；再次按下同一按键才会真正改绑
	UKeyAlreadyBoundWarning* KeyAlreadyBoundWarning = CastChecked<UKeyAlreadyBoundWarning>(
		UCommonUIExtensions::PushContentToLayer_ForPlayer(GetOwningLocalPlayer(), PressAnyKeyLayer, KeyAlreadyBoundWarningPanelClass));

	FString ActionNames;
	for (FName ActionName : ActionsForKey)
	{
		ActionNames += ActionName.ToString() += TEXT("、");
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("InKey"), InKey.GetDisplayName());
	Args.Add(TEXT("ActionNames"), FText::FromString(ActionNames));

	KeyAlreadyBoundWarning->SetWarningText(FText::Format(LOCTEXT("WarningText", "{InKey} 已绑定到 {ActionNames}，确定要重新绑定吗？"), Args));
	KeyAlreadyBoundWarning->SetCancelText(FText::Format(LOCTEXT("CancelText", "按 Esc 取消，或再次按下 {InKey} 确认改绑。"), Args));

	if (InKeyBindSlot == 1)
	{
		KeyAlreadyBoundWarning->OnKeySelected.AddUObject(this, &ThisClass::HandleSecondaryDuplicateKeySelected, KeyAlreadyBoundWarning);
	}
	else
	{
		KeyAlreadyBoundWarning->OnKeySelected.AddUObject(this, &ThisClass::HandlePrimaryDuplicateKeySelected, KeyAlreadyBoundWarning);
	}

	KeyAlreadyBoundWarning->OnKeySelectionCanceled.AddUObject(this, &ThisClass::HandleKeySelectionCanceled, KeyAlreadyBoundWarning);
}

void UYanSettingsListEntrySetting_KeyboardInput::HandleKeySelectionCanceled(UGameSettingPressAnyKey* PressAnyKeyPanel)
{
	PressAnyKeyPanel->OnKeySelectionCanceled.RemoveAll(this);
}

void UYanSettingsListEntrySetting_KeyboardInput::HandleKeySelectionCanceled(UKeyAlreadyBoundWarning* PressAnyKeyPanel)
{
	PressAnyKeyPanel->OnKeySelectionCanceled.RemoveAll(this);
}

void UYanSettingsListEntrySetting_KeyboardInput::HandleClearClicked()
{
	KeyboardInputSetting->ChangeBinding(0, EKeys::Invalid);
	KeyboardInputSetting->ChangeBinding(1, EKeys::Invalid);
}

void UYanSettingsListEntrySetting_KeyboardInput::HandleResetToDefaultClicked()
{
	KeyboardInputSetting->ResetToDefault();
}

void UYanSettingsListEntrySetting_KeyboardInput::OnSettingChanged()
{
	Refresh();
}

void UYanSettingsListEntrySetting_KeyboardInput::Refresh()
{
	if (ensure(KeyboardInputSetting))
	{
		Button_PrimaryKey->SetButtonText(KeyboardInputSetting->GetKeyTextFromSlot(EPlayerMappableKeySlot::First));
		Button_SecondaryKey->SetButtonText(KeyboardInputSetting->GetKeyTextFromSlot(EPlayerMappableKeySlot::Second));

		// 仅在玩家改过键位时才提供恢复默认的入口
		if (ensure(Button_ResetToDefault))
		{
			Button_ResetToDefault->SetVisibility(KeyboardInputSetting->IsMappingCustomized() ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
		}
	}
}

void UYanSettingsListEntrySetting_KeyboardInput::NativeOnEntryReleased()
{
	Super::NativeOnEntryReleased();

	KeyboardInputSetting = nullptr;
}

#undef LOCTEXT_NAMESPACE
