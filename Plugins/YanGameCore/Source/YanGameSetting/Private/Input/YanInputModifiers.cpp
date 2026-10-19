// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/YanInputModifiers.h"

#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerController.h"
#include "Input/YanAimSensitivityData.h"
#include "Settings/YanSettingsShared.h"
#include "Settings/YanSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanInputModifiers)

namespace YanInputModifiersHelpers
{
	// 输入 Modifier 只能拿到 PlayerInput，需沿 Outer 链回溯到本地玩家才能读设置
	static UYanSettingsShared* GetSharedSettings(const UEnhancedPlayerInput* PlayerInput)
	{
		if (PlayerInput)
		{
			if (const APlayerController* PC = Cast<APlayerController>(PlayerInput->GetOuter()))
			{
				return UYanSettingsSubsystem::GetSharedSettingsFor(PC->GetLocalPlayer());
			}
		}

		return nullptr;
	}
}

FInputActionValue UYanSettingBasedScalar::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
	if (ensureMsgf(CurrentValue.GetValueType() != EInputActionValueType::Boolean, TEXT("Setting Based Scalar 不支持布尔类型的输入值")))
	{
		if (UYanSettingsShared* SharedSettings = YanInputModifiersHelpers::GetSharedSettings(PlayerInput))
		{
			const UClass* SettingsClass = UYanSettingsShared::StaticClass();

			const bool bHasCachedProperty = PropertyCache.Num() == 3;

			const FProperty* XAxisValue = bHasCachedProperty ? PropertyCache[0] : SettingsClass->FindPropertyByName(XAxisScalarSettingName);
			const FProperty* YAxisValue = bHasCachedProperty ? PropertyCache[1] : SettingsClass->FindPropertyByName(YAxisScalarSettingName);
			const FProperty* ZAxisValue = bHasCachedProperty ? PropertyCache[2] : SettingsClass->FindPropertyByName(ZAxisScalarSettingName);

			if (PropertyCache.IsEmpty())
			{
				PropertyCache.Emplace(XAxisValue);
				PropertyCache.Emplace(YAxisValue);
				PropertyCache.Emplace(ZAxisValue);
			}

			FVector ScalarToUse = FVector(1.0, 1.0, 1.0);

			switch (CurrentValue.GetValueType())
			{
			case EInputActionValueType::Axis3D:
				ScalarToUse.Z = ZAxisValue ? *ZAxisValue->ContainerPtrToValuePtr<double>(SharedSettings) : 1.0;
				//[[fallthrough]];
			case EInputActionValueType::Axis2D:
				ScalarToUse.Y = YAxisValue ? *YAxisValue->ContainerPtrToValuePtr<double>(SharedSettings) : 1.0;
				//[[fallthrough]];
			case EInputActionValueType::Axis1D:
				ScalarToUse.X = XAxisValue ? *XAxisValue->ContainerPtrToValuePtr<double>(SharedSettings) : 1.0;
				break;
			}

			ScalarToUse.X = FMath::Clamp(ScalarToUse.X, MinValueClamp.X, MaxValueClamp.X);
			ScalarToUse.Y = FMath::Clamp(ScalarToUse.Y, MinValueClamp.Y, MaxValueClamp.Y);
			ScalarToUse.Z = FMath::Clamp(ScalarToUse.Z, MinValueClamp.Z, MaxValueClamp.Z);

			return CurrentValue.Get<FVector>() * ScalarToUse;
		}
	}

	return CurrentValue;
}

FInputActionValue UYanInputModifierAimInversion::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
	const UYanSettingsShared* Settings = YanInputModifiersHelpers::GetSharedSettings(PlayerInput);
	if (!Settings)
	{
		return CurrentValue;
	}

	FVector NewValue = CurrentValue.Get<FVector>();

	if (Settings->GetInvertVerticalAxis())
	{
		NewValue.Y *= -1.0f;
	}

	if (Settings->GetInvertHorizontalAxis())
	{
		NewValue.X *= -1.0f;
	}

	return NewValue;
}

FInputActionValue UYanInputModifierDeadZone::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
	const EInputActionValueType ValueType = CurrentValue.GetValueType();
	if (ValueType == EInputActionValueType::Boolean)
	{
		return CurrentValue;
	}

	const UYanSettingsShared* Settings = YanInputModifiersHelpers::GetSharedSettings(PlayerInput);
	if (!Settings)
	{
		return CurrentValue;
	}

	const float LowerThreshold = FMath::Clamp(
		(DeadzoneStick == EYanDeadzoneStick::MoveStick) ? Settings->GetGamepadMoveStickDeadZone() : Settings->GetGamepadLookStickDeadZone(),
		0.0f, 1.0f);

	// 去掉死区后需要把剩余区间重新映射回 ±1，否则摇杆推满也到不了 1
	auto ApplyDeadZone = [LowerThreshold, this](const float AxisVal)
	{
		return FMath::Min(1.0f, FMath::Max(0.0f, FMath::Abs(AxisVal) - LowerThreshold) / (UpperThreshold - LowerThreshold)) * FMath::Sign(AxisVal);
	};

	FVector NewValue = CurrentValue.Get<FVector>();
	switch (Type)
	{
		case EYanDeadZoneType::Axial:
			NewValue.X = ApplyDeadZone(NewValue.X);
			NewValue.Y = ApplyDeadZone(NewValue.Y);
			NewValue.Z = ApplyDeadZone(NewValue.Z);
			break;

		case EYanDeadZoneType::Radial:
			if (ValueType == EInputActionValueType::Axis3D)
			{
				NewValue = NewValue.GetSafeNormal() * ApplyDeadZone(NewValue.Size());
			}
			else if (ValueType == EInputActionValueType::Axis2D)
			{
				NewValue = NewValue.GetSafeNormal2D() * ApplyDeadZone(NewValue.Size2D());
			}
			else
			{
				NewValue.X = ApplyDeadZone(NewValue.X);
			}
			break;
	}

	return NewValue;
}

FInputActionValue UYanInputModifierGamepadSensitivity::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
	if (CurrentValue.GetValueType() == EInputActionValueType::Boolean || !SensitivityLevelTable)
	{
		return CurrentValue;
	}

	const UYanSettingsShared* Settings = YanInputModifiersHelpers::GetSharedSettings(PlayerInput);
	if (!Settings)
	{
		return CurrentValue;
	}

	const EYanGamepadSensitivity Sensitivity = (TargetingType == EYanTargetingType::Normal)
		                                           ? Settings->GetGamepadLookSensitivityPreset()
		                                           : Settings->GetGamepadTargetingSensitivityPreset();

	return CurrentValue.Get<FVector>() * SensitivityLevelTable->SensitivityEnumToFloat(Sensitivity);
}
