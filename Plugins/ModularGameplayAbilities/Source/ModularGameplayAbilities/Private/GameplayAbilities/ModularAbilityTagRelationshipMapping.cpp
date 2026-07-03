// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayAbilities/ModularAbilityTagRelationshipMapping.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularAbilityTagRelationshipMapping)

void UModularAbilityTagRelationshipMapping::GetAbilityTagsToBlockAndCancel(const FGameplayTagContainer& AbilityTags, FGameplayTagContainer* OutTagsToBlock, FGameplayTagContainer* OutTagsToCancel) const
{
	// 当前实现：线性扫描；后续可换为 Tag 索引加速。
	for (int32 i = 0; i < AbilityTagRelationships.Num(); i++)
	{
		if (const auto& [AbilityTag, AbilityTagsToBlock, AbilityTagsToCancel, ActivationRequiredTags, ActivationBlockedTags] = AbilityTagRelationships[i];
			AbilityTags.HasTag(AbilityTag))
		{
			if (OutTagsToBlock)
			{
				OutTagsToBlock->AppendTags(AbilityTagsToBlock);
			}
			if (OutTagsToCancel)
			{
				OutTagsToCancel->AppendTags(AbilityTagsToCancel);
			}
		}
	}
}

void UModularAbilityTagRelationshipMapping::GetRequiredAndBlockedActivationTags(const FGameplayTagContainer& AbilityTags, FGameplayTagContainer* OutActivationRequired, FGameplayTagContainer* OutActivationBlocked) const
{
	for (int32 i = 0; i < AbilityTagRelationships.Num(); i++)
	{
		if (const auto& [AbilityTag, AbilityTagsToBlock, AbilityTagsToCancel, ActivationRequiredTags, ActivationBlockedTags] = AbilityTagRelationships[i];
			AbilityTags.HasTag(AbilityTag))
		{
			if (OutActivationRequired)
			{
				OutActivationRequired->AppendTags(ActivationRequiredTags);
			}
			if (OutActivationBlocked)
			{
				OutActivationBlocked->AppendTags(ActivationBlockedTags);
			}
		}
	}
}

bool UModularAbilityTagRelationshipMapping::IsAbilityCancelledByTag(const FGameplayTagContainer& AbilityTags, const FGameplayTag& ActionTag) const
{
	for (int32 i = 0; i < AbilityTagRelationships.Num(); i++)
	{
		if (const auto& [AbilityTag, AbilityTagsToBlock, AbilityTagsToCancel, ActivationRequiredTags, ActivationBlockedTags] = AbilityTagRelationships[i];
			AbilityTag == ActionTag && AbilityTagsToCancel.HasAny(AbilityTags))
		{
			return true;
		}
	}

	return false;
}

