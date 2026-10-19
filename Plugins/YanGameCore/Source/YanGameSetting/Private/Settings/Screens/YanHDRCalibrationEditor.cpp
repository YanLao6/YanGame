// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/Screens/YanHDRCalibrationEditor.h"

#include "ColorManagement/TransferFunctions.h"
#include "CommonButtonBase.h"
#include "Components/Image.h"
#include "GameSettingValueScalar.h"
#include "HAL/IConsoleManager.h"
#include "Settings/YanSettingsLocal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanHDRCalibrationEditor)

UYanHDRCalibrationEditor::UYanHDRCalibrationEditor(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
}

void UYanHDRCalibrationEditor::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (IConsoleVariable* UILevel = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HDR.UI.Level")))
	{
		StartingUILevel = UILevel->GetFloat();
		UILevel->SetWithCurrentPriority(1.0f);
	}
	UYanSettingsLocal* Settings = UYanSettingsLocal::Get();
	StartingUILuminance = Settings->GetHDRUILuminanceNits();
	bStartingUILuminanceSeparate = Settings->IsHDRUILuminanceSeparate();
	Settings->SetHDRUILuminanceNits(10000.0f);
	Settings->SetHDRUILuminanceSeparate(true);
	const float MaxLuminance = FMath::Clamp(Settings->GetMaximumHDRDisplayNits() / 10000.0f, 0.0f, 1.0f);
	MaxLuminancePQ = UE::Color::EncodeNormalizedToST2084(MaxLuminance);
	OnMaxLuminanceChange(MaxLuminance);
	Button_Done->OnClicked().AddUObject(this, &ThisClass::HandleDoneClicked);
	Button_Back->SetVisibility(bCanCancel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bCanCancel)
	{
		Button_Back->OnClicked().AddUObject(this, &ThisClass::HandleBackClicked);
	}
}

void UYanHDRCalibrationEditor::NativeOnDeactivated()
{
	Button_Back->OnClicked().RemoveAll(this);
	Button_Done->OnClicked().RemoveAll(this);
	if (IConsoleVariable* UILevel = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HDR.UI.Level")))
	{
		UILevel->SetWithCurrentPriority(StartingUILevel);
	}
	UYanSettingsLocal* Settings = UYanSettingsLocal::Get();
	Settings->SetHDRUILuminanceNits(StartingUILuminance);
	Settings->SetHDRUILuminanceSeparate(bStartingUILuminanceSeparate);
	Super::NativeOnDeactivated();
}

bool UYanHDRCalibrationEditor::ExecuteActionForSetting_Implementation(FGameplayTag ActionTag, UGameSetting* InSetting)
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

void UYanHDRCalibrationEditor::AdjustLuminance(float Delta)
{
	MaxLuminancePQ = FMath::Clamp(MaxLuminancePQ + Delta, 0.51f, 1.0f);
	OnMaxLuminanceChange(UE::Color::DecodeNormalizedFromST2084(MaxLuminancePQ));
}

FReply UYanHDRCalibrationEditor::NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent)
{
	if (InAnalogEvent.GetKey() == EKeys::Gamepad_LeftY && FMath::Abs(InAnalogEvent.GetAnalogValue()) >= 0.2f)
	{
		AdjustLuminance(InAnalogEvent.GetAnalogValue() * 0.01f);
		return FReply::Handled();
	}
	return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogEvent);
}

FReply UYanHDRCalibrationEditor::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	AdjustLuminance(InMouseEvent.GetWheelDelta() * 0.01f);
	return FReply::Handled();
}

void UYanHDRCalibrationEditor::HandleBackClicked()
{
	DeactivateWidget();
}

void UYanHDRCalibrationEditor::HandleDoneClicked()
{
	const float MaxLuminance = UE::Color::DecodeST2084(MaxLuminancePQ);
	if (ValueSetting.IsValid())
	{
		ValueSetting->SetValue(MaxLuminance);
	}
	else
	{
		UYanSettingsLocal::Get()->SetMaximumHDRDisplayNits(FMath::RoundToInt(MaxLuminance));
	}
	// 临时 UI 校准亮度必须先恢复，再保存用户设置。
	DeactivateWidget();
	if (!ValueSetting.IsValid())
	{
		UYanSettingsLocal::Get()->SaveSettings();
	}
}
