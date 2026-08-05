// Copyright Chronicler.

#include "ModularExperienceUtils.h"

#include "ActorComponent/ModularInputConfigComponent.h"
#include "CommonInputBaseTypes.h"
#include "CommonInputSubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularExperienceUtils)

namespace
{
	// 向 Pawn 的输入组件解析 Tag：PawnData 与 GameFeature 注入的 InputConfig 都在该组件上登记，
	// 故此处不经 PawnData，PawnData 是否配置输入 Fragment 与查询结果无关。
	const UInputAction* FindInputActionForTag(const APlayerController* PlayerController, const FGameplayTag& InputTag)
	{
		const APawn* Pawn = PlayerController->GetPawn();
		if (Pawn == nullptr)
		{
			return nullptr;
		}

		const UModularInputConfigComponent* InputConfigComponent = Cast<UModularInputConfigComponent>(Pawn->InputComponent);
		if (InputConfigComponent == nullptr)
		{
			return nullptr;
		}

		return InputConfigComponent->FindInputActionForTag(InputTag);
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
