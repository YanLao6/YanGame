// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "GameplayAbilities/ModularAbilityCost.h"
#include "ScalableFloat.h"

#include "AbilityCost_ItemTagStack.generated.h"

#define UE_API EQUIPMANAGER_API

struct FGameplayAbilityActivationInfo;
struct FGameplayAbilitySpecHandle;

class UModularGameplayAbility;
struct FGameplayAbilityActorInfo;

/**
 * 消耗技能所属装备对应物品实例上 Tag Stack 层数的 Cost。
 * 仅对 UGameplayAbility_FromEquipment 派生的技能有效，其余技能一律判定为不可支付。
 */
UCLASS(MinimalAPI, meta = (DisplayName = "物品 Tag Stack"))
class UAbilityCost_ItemTagStack : public UModularAbilityCost
{
	GENERATED_BODY()

public:
	/** 默认数量 1，失败标签取通用的 Cost 失败标签。 */
	UE_API UAbilityCost_ItemTagStack();

	//~Begin UModularAbilityCost Interface
	UE_API virtual bool CheckCost(const UModularGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const override;
	UE_API virtual void ApplyCost(const UModularGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	//~End UModularAbilityCost Interface

protected:
	/** 消耗层数（随 AbilityLevel 缩放）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Costs)
	FScalableFloat Quantity;

	/** 消耗的目标 Tag。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Costs)
	FGameplayTag Tag;

	/** 无法支付时回传给调用方的标签，供 UI 区分失败原因。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Costs)
	FGameplayTag FailureTag;
};

#undef UE_API
