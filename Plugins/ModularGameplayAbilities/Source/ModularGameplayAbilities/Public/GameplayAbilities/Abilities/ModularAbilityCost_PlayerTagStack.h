// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "GameplayAbilities/ModularAbilityCost.h"
#include "ScalableFloat.h"

#include "ModularAbilityCost_PlayerTagStack.generated.h"

#define UE_API MODULARGAMEPLAYABILITIES_API

struct FGameplayAbilityActivationInfo;
struct FGameplayAbilitySpecHandle;

class UModularGameplayAbility;
class UObject;
struct FGameplayAbilityActorInfo;

/**
 * 消耗 PlayerState 上 FGameplayTagStack 层数的 Cost。
 */
UCLASS(MinimalAPI, meta=(DisplayName="玩家 Tag Stack"))
class UModularAbilityCost_PlayerTagStack : public UModularAbilityCost
{
	GENERATED_BODY()

public:
	/** 默认数量 1。 */
	UE_API UModularAbilityCost_PlayerTagStack();

	//~UModularAbilityCost 接口
	UE_API virtual bool CheckCost(const UModularGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const override;
	UE_API virtual void ApplyCost(const UModularGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	//~UModularAbilityCost 接口结束

protected:
	/** 消耗层数（随 AbilityLevel 缩放，FScalableFloat）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Costs)
	FScalableFloat Quantity;

	/** 目标 GameplayTag。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Costs)
	FGameplayTag Tag;
};

#undef UE_API
