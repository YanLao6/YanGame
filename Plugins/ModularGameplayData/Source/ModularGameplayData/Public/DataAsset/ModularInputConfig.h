// Copyright Chronicler.


#pragma once

#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "ModularInputConfig.generated.h"

/** 将一条 UInputAction 映射到 GameplayTag（InputTag）。 */
USTRUCT(BlueprintType)
struct FModularInputConfigAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

UCLASS(BlueprintType, Const)
class MODULARGAMEPLAYDATA_API UModularInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	explicit UModularInputConfig(const FObjectInitializer& ObjectInitializer);

	/** 在 NativeInputActions 中按 InputTag 查找 UInputAction；未找到时可打日志。 */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputGameplayTag, bool bLogNotFound = true) const;

	/** 在 AbilityInputActions 中按 InputTag 查找供 UGameplayAbility 绑定的 UInputAction。 */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputGameplayTag, bool bLogNotFound = true) const;

	/**
	 * Native 输入：UInputAction 与 GameplayTag 对应表。
	 * 需在 PlayerInput/自定义组件中手动 BindAction。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (TitleProperty = "InputAction"))
	TArray<FModularInputConfigAction> NativeInputActions;

	/**
	 * Ability 输入：与 Ability 上 InputTag 匹配的 UInputAction。
	 * 通常由 ModularInputConfigComponent::BindAbilityActions 批量绑定。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (TitleProperty = "InputAction"))
	TArray<FModularInputConfigAction> AbilityInputActions;
};
