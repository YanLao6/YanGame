#include "HeroSelectionSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HeroSelectionSubsystem)

void UHeroSelectionSubsystem::SetSelectedHero(const FUniqueNetIdRepl& PlayerId, const FPrimaryAssetId& HeroId)
{
	if (!PlayerId.IsValid())
	{
		return;
	}

	if (HeroId.IsValid())
	{
		SelectionsByPlayer.Add(PlayerId, HeroId);
	}
	else
	{
		SelectionsByPlayer.Remove(PlayerId);
	}
}

FPrimaryAssetId UHeroSelectionSubsystem::GetSelectedHero(const FUniqueNetIdRepl& PlayerId) const
{
	if (!PlayerId.IsValid())
	{
		return FPrimaryAssetId();
	}

	if (const FPrimaryAssetId* Found = SelectionsByPlayer.Find(PlayerId))
	{
		return *Found;
	}

	return FPrimaryAssetId();
}

void UHeroSelectionSubsystem::ClearSelection(const FUniqueNetIdRepl& PlayerId)
{
	SelectionsByPlayer.Remove(PlayerId);
}

void UHeroSelectionSubsystem::ClearAllSelections()
{
	SelectionsByPlayer.Empty();
}
