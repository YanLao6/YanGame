// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayAbilities/ModularAbilityCost.h"
#include "ScalableFloat.h"
#include "Templates/SubclassOf.h"

#include "AbilityCost_InventoryItem.generated.h"

#define UE_API EQUIPMANAGER_API

struct FGameplayAbilityActivationInfo;
struct FGameplayAbilitySpecHandle;

class UModularGameplayAbility;
class UInventoryItemDefinition;
struct FGameplayAbilityActorInfo;
struct FGameplayTagContainer;

/**
 * 消耗背包中若干数量某种物品的 Cost。
 * 背包组件挂在 Controller 上，因此该 Cost 只对具备 Controller 的施法者有效。
 */
UCLASS(MinimalAPI, meta = (DisplayName = "背包物品"))
class UAbilityCost_InventoryItem : public UModularAbilityCost
{
	GENERATED_BODY()

public:
	/** 默认数量 1。 */
	UE_API UAbilityCost_InventoryItem();

	//~Begin UModularAbilityCost Interface
	UE_API virtual bool CheckCost(const UModularGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const override;
	UE_API virtual void ApplyCost(const UModularGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	//~End UModularAbilityCost Interface

protected:
	/** 消耗数量（随 AbilityLevel 缩放）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AbilityCost)
	FScalableFloat Quantity;

	/** 消耗的物品类型。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AbilityCost)
	TSubclassOf<UInventoryItemDefinition> ItemDefinition;
};

#undef UE_API
