// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "ModularAbilityTagRelationshipMapping.generated.h"

class UObject;

/** 描述某个 AbilityTag 与其它 Tag 的 Block / Cancel / 激活条件关系。 */
USTRUCT()
struct FModularAbilityTagRelationship
{
	GENERATED_BODY()

	/** 本条关系所针对的单个 AbilityTag（Ability 可带多个此类 Tag）。 */
	UPROPERTY(EditAnywhere, Category = Ability, meta = (Categories = "Gameplay.Action"))
	FGameplayTag AbilityTag;

	/** 当 Ability 携带 AbilityTag 时，将阻塞这些 AbilityTag。 */
	UPROPERTY(EditAnywhere, Category = Ability)
	FGameplayTagContainer AbilityTagsToBlock;

	/** 当 Ability 携带 AbilityTag 时，将取消这些 AbilityTag 对应的 Ability。 */
	UPROPERTY(EditAnywhere, Category = Ability)
	FGameplayTagContainer AbilityTagsToCancel;

	/** 隐式并入 Ability 的 ActivationRequiredTags。 */
	UPROPERTY(EditAnywhere, Category = Ability)
	FGameplayTagContainer ActivationRequiredTags;

	/** 隐式并入 Ability 的 ActivationBlockedTags。 */
	UPROPERTY(EditAnywhere, Category = Ability)
	FGameplayTagContainer ActivationBlockedTags;
};


/** AbilityTag 互斥与激活约束的数据资产。 */
UCLASS()
class UModularAbilityTagRelationshipMapping : public UDataAsset
{
	GENERATED_BODY()

private:
	/** 全部 Tag 关系条目。 */
	UPROPERTY(EditAnywhere, Category = Ability, meta=(TitleProperty="AbilityTag"))
	TArray<FModularAbilityTagRelationship> AbilityTagRelationships;

public:
	/** 根据 AbilityTags 汇总 Block / Cancel Tag（写入可选 Out 指针）。 */
	void GetAbilityTagsToBlockAndCancel(const FGameplayTagContainer& AbilityTags, FGameplayTagContainer* OutTagsToBlock, FGameplayTagContainer* OutTagsToCancel) const;

	/** 根据 AbilityTags 汇总额外 Required / Blocked 激活 Tag。 */
	void GetRequiredAndBlockedActivationTags(const FGameplayTagContainer& AbilityTags, FGameplayTagContainer* OutActivationRequired, FGameplayTagContainer* OutActivationBlocked) const;

	/** ActionTag 是否命中某条关系并 Cancel 了 AbilityTags。 */
	bool IsAbilityCancelledByTag(const FGameplayTagContainer& AbilityTags, const FGameplayTag& ActionTag) const;
};
