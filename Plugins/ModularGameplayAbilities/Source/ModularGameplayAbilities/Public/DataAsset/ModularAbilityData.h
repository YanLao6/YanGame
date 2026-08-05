// Copyright Chronicler.

#pragma once

#include "GameplayEffect.h"
#include "Engine/DataAsset.h"

#include "ModularAbilityData.generated.h"

/**
 * 全局 Modular Ability 数据资产（伤害 / 治疗 / 动态 Tag 等默认 GE 引用）。
 */
UCLASS(MinimalAPI, BlueprintType, Const, meta=(DisplayName="Modular Ability 数据", ShortTooltip="全局 Modular Ability 数据：默认 GameplayEffect 引用等。"))
class UModularAbilityData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 默认构造。 */
	UModularAbilityData();

	/** 通过 UModularGameplayAbilitiesConfig 加载或取得全局单例数据。 */
	static const UModularAbilityData& Get();

	// 伤害用默认 GE（SetByCaller 传递 Magnitude）。
	UPROPERTY(EditDefaultsOnly, Category = "Default Gameplay Effects", meta = (DisplayName = "伤害 GE (SetByCaller)"))
	TSoftClassPtr<UGameplayEffect> DamageGameplayEffect_SetByCaller;

	// 治疗用默认 GE（SetByCaller 传递 Magnitude）。
	UPROPERTY(EditDefaultsOnly, Category = "Default Gameplay Effects", meta = (DisplayName = "治疗 GE (SetByCaller)"))
	TSoftClassPtr<UGameplayEffect> HealGameplayEffect_SetByCaller;

	// 动态添加/移除 GameplayTag 时使用的默认 GE。
	UPROPERTY(EditDefaultsOnly, Category = "Default Gameplay Effects")
	TSoftClassPtr<UGameplayEffect> DynamicTagGameplayEffect;
};
