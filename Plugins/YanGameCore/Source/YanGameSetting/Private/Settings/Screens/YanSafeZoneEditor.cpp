// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/Screens/YanSafeZoneEditor.h"

#include "CommonButtonBase.h"
#include "CommonInputSubsystem.h"
#include "CommonRichTextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "GameSettingValueScalar.h"
#include "Settings/YanSettingsLocal.h"
#include "Widgets/Layout/SSafeZone.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanSafeZoneEditor)

#define LOCTEXT_NAMESPACE "YanSafeZoneEditor"

UYanSafeZoneEditor::UYanSafeZoneEditor(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
}

void UYanSafeZoneEditor::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Switcher_SafeZoneMessage->SetActiveWidget(RichText_Default);
}

void UYanSafeZoneEditor::NativeOnActivated()
{
	Super::NativeOnActivated();
	StartingSafeZone = UYanSettingsLocal::Get()->GetSafeZone();
	bConfirmed = false;
	SSafeZone::SetGlobalSafeZoneScale(StartingSafeZone);
	Button_Done->OnClicked().AddUObject(this, &ThisClass::HandleDoneClicked);
	Button_Back->SetVisibility(bCanCancel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bCanCancel)
	{
		Button_Back->OnClicked().AddUObject(this, &ThisClass::HandleBackClicked);
	}
	if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		Input->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleInputModeChanged);
		HandleInputModeChanged(Input->GetCurrentInputType());
	}
}

void UYanSafeZoneEditor::NativeOnDeactivated()
{
	Button_Back->OnClicked().RemoveAll(this);
	Button_Done->OnClicked().RemoveAll(this);
	if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		Input->OnInputMethodChangedNative.RemoveAll(this);
	}
	if (!bConfirmed)
	{
		SSafeZone::SetGlobalSafeZoneScale(StartingSafeZone);
	}
	Super::NativeOnDeactivated();
}

bool UYanSafeZoneEditor::ExecuteActionForSetting_Implementation(FGameplayTag ActionTag, UGameSetting* InSetting)
{
	ValueSetting.Reset();
	if (InSetting)
	{
		const TArray<UGameSetting*> Children = InSetting->GetChildSettings();
		if (!Children.IsEmpty())
		{
			ValueSetting = Cast<UGameSettingValueScalar>(Children[0]);
		}
	}
	return true;
}

void UYanSafeZoneEditor::AdjustSafeZone(float Delta)
{
	SSafeZone::SetGlobalSafeZoneScale(FMath::Clamp(SSafeZone::GetGlobalSafeZoneScale().Get(0.0f) + Delta, 0.0f, 1.0f));
}

FReply UYanSafeZoneEditor::NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent)
{
	if (InAnalogEvent.GetKey() == EKeys::Gamepad_LeftY && FMath::Abs(InAnalogEvent.GetAnalogValue()) >= 0.2f)
	{
		AdjustSafeZone(InAnalogEvent.GetAnalogValue() * 0.1f);
		return FReply::Handled();
	}
	return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogEvent);
}

FReply UYanSafeZoneEditor::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	AdjustSafeZone(InMouseEvent.GetWheelDelta() * 0.1f);
	return FReply::Handled();
}

void UYanSafeZoneEditor::HandleInputModeChanged(ECommonInputType InInputType)
{
	const FText InputName = InInputType == ECommonInputType::Gamepad ? LOCTEXT("Gamepad", "左摇杆") : LOCTEXT("Mouse", "鼠标滚轮");
	RichText_Default->SetText(FText::Format(LOCTEXT("Instructions", "使用 <text color=\"FFFFFFFF\" fontface=\"black\">{0}</> 调整边角，使其与显示器边缘对齐。"), InputName));
}

void UYanSafeZoneEditor::HandleBackClicked()
{
	DeactivateWidget();
}

void UYanSafeZoneEditor::HandleDoneClicked()
{
	const float Value = SSafeZone::GetGlobalSafeZoneScale().Get(0.0f);
	if (ValueSetting.IsValid())
	{
		ValueSetting->SetValue(Value);
	}
	else
	{
		UYanSettingsLocal::Get()->SetSafeZone(Value);
		UYanSettingsLocal::Get()->SaveSettings();
	}
	bConfirmed = true;
	OnSafeZoneSet.Broadcast();
	DeactivateWidget();
}

#undef LOCTEXT_NAMESPACE
