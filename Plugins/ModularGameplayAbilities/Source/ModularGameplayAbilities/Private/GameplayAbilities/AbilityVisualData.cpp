// Copyright Chronicler.

#include "GameplayAbilities/AbilityVisualData.h"

#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityVisualData)

bool UAbilityVisualData::MatchesAbilityTag(FGameplayTag InAbilityTag) const
{
	return AbilityTag.IsValid() && AbilityTag.MatchesTag(InAbilityTag);
}

UTexture2D* UAbilityVisualData::GetLoadedIcon() const
{
	return Icon.Get();
}

UTexture2D* UAbilityVisualData::LoadIcon() const
{
	return Icon.IsNull() ? nullptr : Icon.LoadSynchronous();
}

UAbilityVisualData* UAbilityVisualData::FindByAbilityTag(const TArray<UAbilityVisualData*>& VisualDataList, FGameplayTag InAbilityTag)
{
	if (!InAbilityTag.IsValid())
	{
		return nullptr;
	}

	for (UAbilityVisualData* VisualData : VisualDataList)
	{
		if (VisualData != nullptr && VisualData->MatchesAbilityTag(InAbilityTag))
		{
			return VisualData;
		}
	}

	return nullptr;
}
