#include "HeroCatalog.h"

#include "DataAsset/ModularPawnData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HeroCatalog)

FPrimaryAssetId UHeroCatalog::GetHeroIdAt(int32 Index) const
{
	if (!Heroes.IsValidIndex(Index) || !Heroes[Index].PawnData)
	{
		return FPrimaryAssetId();
	}

	return Heroes[Index].PawnData->GetPrimaryAssetId();
}

int32 UHeroCatalog::IndexOfHero(FPrimaryAssetId HeroId) const
{
	if (!HeroId.IsValid())
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < Heroes.Num(); ++Index)
	{
		if (Heroes[Index].PawnData && Heroes[Index].PawnData->GetPrimaryAssetId() == HeroId)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

const UModularPawnData* UHeroCatalog::FindPawnData(const FPrimaryAssetId& HeroId) const
{
	if (!HeroId.IsValid())
	{
		return nullptr;
	}

	for (const FHeroEntry& Entry : Heroes)
	{
		if (Entry.PawnData && Entry.PawnData->GetPrimaryAssetId() == HeroId)
		{
			return Entry.PawnData.Get();
		}
	}

	return nullptr;
}

bool UHeroCatalog::IsEmpty() const
{
	for (const FHeroEntry& Entry : Heroes)
	{
		if (Entry.PawnData)
		{
			return false;
		}
	}

	return true;
}
