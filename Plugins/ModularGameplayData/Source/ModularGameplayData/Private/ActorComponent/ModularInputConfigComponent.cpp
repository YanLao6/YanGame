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

void UModularInputConfigComponent::RemoveBinds(TArray<uint32>& BindHandles)
{
	for (const uint32 Handle : BindHandles)
	{
		RemoveBindingByHandle(Handle);
	}
	BindHandles.Reset();
}
