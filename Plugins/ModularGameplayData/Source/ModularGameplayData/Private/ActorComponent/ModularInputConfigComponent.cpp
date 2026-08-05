// Copyright Chronicler.

#include "ActorComponent/ModularInputConfigComponent.h"
#include "EnhancedInputSubsystems.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularInputConfigComponent)

const FName UModularInputConfigComponent::NAME_BindInputsNow("BindInputsNow");

UModularInputConfigComponent::UModularInputConfigComponent(const FObjectInitializer& ObjectInitializer)
{
}

void UModularInputConfigComponent::AddInputMappings(const UModularInputConfig* InputConfig, const UEnhancedInputLocalPlayerSubsystem* InputSubsystem)
{
	check(InputConfig);
	check(InputSubsystem);

	// 项目可扩展：从 InputConfig 额外添加 IMC 或其它映射逻辑
}

void UModularInputConfigComponent::RemoveInputMappings(const UModularInputConfig* InputConfig, const UEnhancedInputLocalPlayerSubsystem* InputSubsystem)
{
	check(InputConfig);
	check(InputSubsystem);

	// 项目可扩展：与 AddInputMappings 对称，移除自定义添加的映射
}

void UModularInputConfigComponent::RegisterInputConfig(const UModularInputConfig* InputConfig)
{
	if (!InputConfig)
	{
		return;
	}

	// 调用方持有的均为 const 视图，容器需非 const 元素类型以满足 UPROPERTY 反射要求。
	ActiveInputConfigs.Add(const_cast<UModularInputConfig*>(InputConfig));
}

void UModularInputConfigComponent::UnregisterInputConfig(const UModularInputConfig* InputConfig)
{
	if (!InputConfig)
	{
		return;
	}

	ActiveInputConfigs.RemoveSingle(const_cast<UModularInputConfig*>(InputConfig));
}

const UInputAction* UModularInputConfigComponent::FindInputActionForTag(const FGameplayTag& InputTag) const
{
	if (!InputTag.IsValid())
	{
		return nullptr;
	}

	for (const UModularInputConfig* InputConfig : ActiveInputConfigs)
	{
		if (InputConfig)
		{
			if (const UInputAction* AbilityAction = InputConfig->FindAbilityInputActionForTag(InputTag, /*bLogNotFound=*/ false))
			{
				return AbilityAction;
			}
		}
	}

	for (const UModularInputConfig* InputConfig : ActiveInputConfigs)
	{
		if (InputConfig)
		{
			if (const UInputAction* NativeAction = InputConfig->FindNativeInputActionForTag(InputTag, /*bLogNotFound=*/ false))
			{
				return NativeAction;
			}
		}
	}

	return nullptr;
}

void UModularInputConfigComponent::RemoveBinds(TArray<uint32>& BindHandles)
{
	for (const uint32 Handle : BindHandles)
	{
		RemoveBindingByHandle(Handle);
	}
	BindHandles.Reset();
}
