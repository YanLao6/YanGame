// Copyright Chronicler.


#pragma once

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "DataAsset/ModularInputConfig.h"
#include "ModularInputConfigComponent.generated.h"

#define UE_API MODULARGAMEPLAYDATA_API

/**
 * Input 配置组件。
 *
 * 使用 `UModularInputConfig` 统一管理 EnhancedInput 的 mapping 与 action bind。
 * 可在 Project Settings 中配置，也可在运行时动态绑定。
 *
 * 设计参考：Lyra 示例中的 `ULyraInputComponent`（路径 LyraGame/Input）。
 */
UCLASS(MinimalAPI, Config="Input")
class UModularInputConfigComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	static UE_API const FName NAME_BindInputsNow;
	/** 构造输入配置组件。 */
	UE_API explicit UModularInputConfigComponent(const FObjectInitializer& ObjectInitializer);

	/** 根据输入配置添加 InputMappingContexts。 */
	UE_API void AddInputMappings(const UModularInputConfig* InputConfig, const UEnhancedInputLocalPlayerSubsystem* InputSubsystem);
	/** 根据输入配置移除 InputMappingContexts。 */
	UE_API void RemoveInputMappings(const UModularInputConfig* InputConfig, const UEnhancedInputLocalPlayerSubsystem* InputSubsystem);

	/**
	 * 登记一份在本组件上建立了绑定的 InputConfig，使其可被 Tag 反查。
	 *
	 * 绑定句柄无法反推来源资产，故查询侧需要这份独立记录。所有绑定路径（PawnData 与
	 * GameFeature 运行期注入）都经过本组件，此处即全部生效配置的唯一汇聚点。
	 * 同一 InputConfig 可被多个组件各绑一次，重复登记按引用计数保留，由 Unregister 逐次抵消。
	 */
	UE_API void RegisterInputConfig(const UModularInputConfig* InputConfig);

	/** 注销一次由 RegisterInputConfig 建立的登记；未登记过的 InputConfig 会被忽略。 */
	UE_API void UnregisterInputConfig(const UModularInputConfig* InputConfig);

	/**
	 * 在全部已登记的 InputConfig 中按 InputTag 反查 UInputAction，未配置时返回 nullptr。
	 *
	 * Ability 表整体优先于 Native 表：同一 Tag 若在两类表中重复出现，以 Ability 语义为准。
	 */
	UE_API const UInputAction* FindInputActionForTag(const FGameplayTag& InputTag) const;

	/** 按 InputTag 绑定原生输入回调；返回绑定句柄，InputConfig 中无此 InputTag 时返回 0。 */
	template<class UserClass, typename FuncType>
	uint32 BindNativeAction(const UModularInputConfig* InputConfig,
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
	UE_API void RemoveBinds(TArray<uint32>& BindHandles);

private:
	/** 已在本组件上建立绑定的 InputConfig，允许重复条目以表达引用计数。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UModularInputConfig>> ActiveInputConfigs;
};


template<class UserClass, typename FuncType>
uint32 UModularInputConfigComponent::BindNativeAction(const UModularInputConfig* InputConfig,
	const FGameplayTag& InputTag,
	ETriggerEvent TriggerEvent,
	UserClass* Object,
	FuncType Func,
	bool bLogIfNotFound)
{
	check(InputConfig);
	if (const UInputAction* InputAction = InputConfig->FindNativeInputActionForTag(InputTag, bLogIfNotFound))
	{
		return BindAction(InputAction, TriggerEvent, Object, Func).GetHandle();
	}

	return 0;
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

#undef UE_API
