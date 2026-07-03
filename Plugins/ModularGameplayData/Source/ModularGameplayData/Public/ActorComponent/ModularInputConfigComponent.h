// Copyright Chronicler.


#pragma once

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "DataAsset/ModularInputConfig.h"
#include "ModularInputConfigComponent.generated.h"

/**
 * Input 配置组件。
 *
 * 使用 `UModularInputConfig` 统一管理 EnhancedInput 的 mapping 与 action bind。
 * 可在 Project Settings 中配置，也可在运行时动态绑定。
 *
 * 设计参考：Lyra 示例中的 `ULyraInputComponent`（路径 LyraGame/Input）。
 */
UCLASS(Config="Input")
class MODULARGAMEPLAYDATA_API UModularInputConfigComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	static const FName NAME_BindInputsNow;
	/** 构造输入配置组件。 */
	explicit UModularInputConfigComponent(const FObjectInitializer& ObjectInitializer);

	/** 根据输入配置添加 InputMappingContexts。 */
	void AddInputMappings(const UModularInputConfig* InputConfig, const UEnhancedInputLocalPlayerSubsystem* InputSubsystem);
	/** 根据输入配置移除 InputMappingContexts。 */
	void RemoveInputMappings(const UModularInputConfig* InputConfig, const UEnhancedInputLocalPlayerSubsystem* InputSubsystem);

	/** 按 InputTag 绑定原生输入回调。 */
	template<class UserClass, typename FuncType>
	void BindNativeAction(const UModularInputConfig* InputConfig,
		const FGameplayTag& InputTag,
		ETriggerEvent TriggerEvent,
		UserClass* Object,
		FuncType Func,
		bool bLogIfNotFound);

	/** 批量绑定 Ability 输入动作的 Pressed/Released 回调。 */
	template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
	void BindAbilityActions(const UModularInputConfig* InputConfig,
		UserClass* Object,
		PressedFuncType PressedFunc,
		ReleasedFuncType ReleasedFunc,
		TArray<uint32>& BindHandles);

	/** 按句柄数组移除先前建立的输入绑定。 */
	void RemoveBinds(TArray<uint32>& BindHandles);
};


template<class UserClass, typename FuncType>
void UModularInputConfigComponent::BindNativeAction(const UModularInputConfig* InputConfig,
	const FGameplayTag& InputTag,
	ETriggerEvent TriggerEvent,
	UserClass* Object,
	FuncType Func,
	bool bLogIfNotFound)
{
	check(InputConfig);
	if (const UInputAction* InputAction = InputConfig->FindNativeInputActionForTag(InputTag, bLogIfNotFound))
	{
		BindAction(InputAction, TriggerEvent, Object, Func);
	}
}

template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
void UModularInputConfigComponent::BindAbilityActions(const UModularInputConfig* InputConfig,
	UserClass* Object,
	PressedFuncType PressedFunc,
	ReleasedFuncType ReleasedFunc,
	TArray<uint32>& BindHandles)
{
	check(InputConfig);

	for (const auto& [InputAction, InputTag] : InputConfig->AbilityInputActions)
	{
		if (InputAction && InputTag.IsValid())
		{
			if (PressedFunc)
			{
				BindHandles.Add(BindAction(InputAction, ETriggerEvent::Triggered, Object, PressedFunc, InputTag).GetHandle());
			}

			if (ReleasedFunc)
			{
				BindHandles.Add(BindAction(InputAction, ETriggerEvent::Completed, Object, ReleasedFunc, InputTag).GetHandle());
			}
		}
	}
}
