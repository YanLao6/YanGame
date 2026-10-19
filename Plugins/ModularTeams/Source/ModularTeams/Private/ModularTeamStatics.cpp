// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularTeamStatics.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "ModularTeamDisplayAsset.h"
#include "ModularTeamSubsystem.h"
#include "ModularTeams.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularTeamStatics)

class UTexture;

void UModularTeamStatics::FindTeamFromObject(const UObject* Agent, bool& bIsPartOfTeam, int32& TeamId, UModularTeamDisplayAsset*& DisplayAsset, bool bLogIfNotSet)
{
	bIsPartOfTeam = false;
	TeamId = INDEX_NONE;
	DisplayAsset = nullptr;

	if (UWorld* World = GEngine->GetWorldFromContextObject(Agent, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (UModularTeamSubsystem* TeamSubsystem = World->GetSubsystem<UModularTeamSubsystem>())
		{
			TeamId = TeamSubsystem->FindTeamFromObject(Agent);
			if (TeamId != INDEX_NONE)
			{
				bIsPartOfTeam = true;

				DisplayAsset = TeamSubsystem->GetTeamDisplayAsset(TeamId, INDEX_NONE);

				if ((DisplayAsset == nullptr) && bLogIfNotSet)
				{
					UE_LOG(LogModularTeams, Log, TEXT("FindTeamFromObject(%s) called too early (found team %d but no display asset set yet)"), *GetPathNameSafe(Agent), TeamId);
				}
			}
		}
		else
		{
			UE_LOG(LogModularTeams, Error, TEXT("FindTeamFromObject(%s) failed: Team subsystem does not exist yet"), *GetPathNameSafe(Agent));
		}
	}
}

UModularTeamDisplayAsset* UModularTeamStatics::GetTeamDisplayAsset(const UObject* WorldContextObject, int32 TeamId)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (UModularTeamSubsystem* TeamSubsystem = World->GetSubsystem<UModularTeamSubsystem>())
		{
			return TeamSubsystem->GetTeamDisplayAsset(TeamId, INDEX_NONE);
		}
	}

	return nullptr;
}

float UModularTeamStatics::GetTeamScalarWithFallback(UModularTeamDisplayAsset* DisplayAsset, FName ParameterName, float DefaultValue)
{
	if (DisplayAsset)
	{
		if (float* pValue = DisplayAsset->ScalarParameters.Find(ParameterName))
		{
			return *pValue;
		}
	}

	return DefaultValue;
}

FLinearColor UModularTeamStatics::GetTeamColorWithFallback(UModularTeamDisplayAsset* DisplayAsset, FName ParameterName, FLinearColor DefaultValue)
{
	if (DisplayAsset)
	{
		if (FLinearColor* pColor = DisplayAsset->ColorParameters.Find(ParameterName))
		{
			return *pColor;
		}
	}

	return DefaultValue;
}

UTexture* UModularTeamStatics::GetTeamTextureWithFallback(UModularTeamDisplayAsset* DisplayAsset, FName ParameterName, UTexture* DefaultValue)
{
	if (DisplayAsset)
	{
		if (TObjectPtr<UTexture>* pTexture = DisplayAsset->TextureParameters.Find(ParameterName))
		{
			return *pTexture;
		}
	}

	return DefaultValue;
}
