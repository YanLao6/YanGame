// Copyright Chronicler.

#include "ModularExperienceUtils.h"

#include "ActorComponent/ModularPawnComponent.h"
#include "CommonInputBaseTypes.h"
#include "CommonInputSubsystem.h"
#include "DataAsset/ModularInputConfig.h"
#include "DataAsset/ModularPawnData.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularExperienceUtils)

namespace
{
	// 沿当前 Pawn 的 PawnData→InputConfig 解析 Tag：Ability 表优先，其次 Native 表；未配置时返回 nullptr。
	const UInputAction* FindInputActionForTag(const APlayerController* PlayerController, const FGameplayTag& InputTag)
	{
		const UModularPawnComponent* PawnComponent = UModularPawnComponent::FindModularPawnComponent(PlayerController->GetPawn());
		if (PawnComponent == nullptr)
		{
			return nullptr;
		}

		const UModularPawnData* PawnData = PawnComponent->GetPawnData<UModularPawnData>();
		const UModularInputConfig* InputConfig = (PawnData != nullptr) ? PawnData->InputConfig : nullptr;
		if (InputConfig == nullptr)
		{
			return nullptr;
		}

		if (const UInputAction* AbilityAction = InputConfig->FindAbilityInputActionForTag(InputTag, /*bLogNotFound=*/false))
		{
			return AbilityAction;
		}

		return InputConfig->FindNativeInputActionForTag(InputTag, /*bLogNotFound=*/false);
	}
}

FKey UModularExperienceUtils::GetKeyValueByTag(const APlayerController* PlayerController, FGameplayTag InputTag)
{
	if (PlayerController == nullptr || !InputTag.IsValid())
	{
		return EKeys::Invalid;
	}

	const UInputAction* InputAction = FindInputActionForTag(PlayerController, InputTag);
	if (InputAction == nullptr)
	{
		return EKeys::Invalid;
	}

	const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	if (LocalPlayer == nullptr)
	{
		return EKeys::Invalid;
	}

	const UEnhancedInputLocalPlayerSubsystem* EnhancedInput = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (EnhancedInput == nullptr)
	{
		return EKeys::Invalid;
	}

	// 按键来自玩家当前生效的 IMC，改键或切换映射后即时反映。
	const TArray<FKey> MappedKeys = EnhancedInput->QueryKeysMappedToAction(InputAction);
	if (MappedKeys.Num() == 0)
	{
		return EKeys::Invalid;
	}

	const UCommonInputSubsystem* CommonInput = UCommonInputSubsystem::Get(LocalPlayer);
	const bool bUsingGamepad = (CommonInput != nullptr) && (CommonInput->GetCurrentInputType() == ECommonInputType::Gamepad);
	for (const FKey& Key : MappedKeys)
	{
		if (Key.IsGamepadKey() == bUsingGamepad)
		{
			return Key;
		}
	}

	// 无匹配当前设备的按键时退回首个映射键。
	return MappedKeys[0];
}
