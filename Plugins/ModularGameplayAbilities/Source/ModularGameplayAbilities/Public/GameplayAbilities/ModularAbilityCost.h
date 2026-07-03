// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "Abilities/GameplayAbility.h"
#include "ModularAbilityCost.generated.h"

class UModularGameplayAbility;

/**
 * UModularGameplayAbility 的可实例化 Cost 基类（弹药、层数等）。
 */
UCLASS(DefaultToInstanced, EditInlineNew, Abstract)
class MODULARGAMEPLAYABILITIES_API UModularAbilityCost : public UObject
{
	GENERATED_BODY()

public:
	/** 默认构造。 */
	UModularAbilityCost()
	{
	}

	/**
	 * 激活前校验是否支付得起 Cost。
	 *
	 * 失败时可在 OptionalRelevantTags 中追加原因 Tag，用于 UI / 音效反馈。
	 * Ability 与 ActorInfo 非空；OptionalRelevantTags 可为 nullptr。
	 *
	 * @return true 表示可通过 CheckCost。
	 */
	virtual bool CheckCost(const UModularGameplayAbility* Ability,
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags) const
	{
		return true;
	}

	/**
	 * 激活后扣除 Cost。
	 *
	 * 说明：
	 * - 无需自行判断 ShouldOnlyApplyCostOnHit()，调用方会处理。
	 * - Ability 与 ActorInfo 非空。
	 */
	virtual void ApplyCost(const UModularGameplayAbility* Ability,
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo)
	{
	}

	/** 若为 true，仅在 Ability 判定命中目标后才 ApplyCost。 */
	bool ShouldOnlyApplyCostOnHit() const { return bOnlyApplyCostOnHit; }

protected:
	/** 同 ShouldOnlyApplyCostOnHit。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Costs)
	bool bOnlyApplyCostOnHit = false;
};
