// Copyright Chronicler.

#pragma once

#include "AbilitySystemGlobals.h"

#include "ModularAbilitySystemGlobals.generated.h"

struct FGameplayEffectContext;

/**
 * 能力系统全局配置。
 *
 * 覆盖效果上下文的分配方式，使全项目所有 GameplayEffect 都使用 FModularGameplayEffectContext。
 * 需在 DefaultGame.ini 的 [/Script/GameplayAbilities.AbilitySystemGlobals] 下注册本类才会生效。
 */
UCLASS(Config = Game)
class UModularAbilitySystemGlobals : public UAbilitySystemGlobals
{
	GENERATED_UCLASS_BODY()

	//~Begin UAbilitySystemGlobals Interface
	virtual FGameplayEffectContext* AllocGameplayEffectContext() const override;
	//~End UAbilitySystemGlobals Interface
};
