// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ModularGameplayUIUtils.h"

#include "CommonInputBaseTypes.h"
#include "CommonInputSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Styling/SlateBrush.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularGameplayUIUtils)

bool UModularGameplayUIUtils::GetKeyBrush(const APlayerController* PlayerController, FKey Key, FSlateBrush& OutBrush)
{
	if (PlayerController == nullptr || !Key.IsValid())
	{
		return false;
	}

	const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	const UCommonInputSubsystem* CommonInput = (LocalPlayer != nullptr) ? UCommonInputSubsystem::Get(LocalPlayer) : nullptr;
	if (CommonInput == nullptr)
	{
		return false;
	}

	// 当前设备的 ControllerData 是否为该按键配了图标 brush；有图则回填供 UImage 直接显示，无图由调用方退回文字提示。
	return UCommonInputPlatformSettings::Get()->TryGetInputBrush(OutBrush, Key, CommonInput->GetCurrentInputType(), CommonInput->GetCurrentGamepadName());
}
