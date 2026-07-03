// Copyright Chronicler.


#include "GameMode/ModularWorldSettings.h"

#include "ModularGameplayExperiencesLogs.h"
#include "Engine/AssetManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularWorldSettings)

AModularWorldSettings::AModularWorldSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

AModularWorldSettings* AModularWorldSettings::GetModularWorldSettings(const UObject* WorldContent)
{
	return Cast<AModularWorldSettings>(WorldContent->GetWorld()->GetWorldSettings());
}

FPrimaryAssetId AModularWorldSettings::GetDefaultGameplayExperience() const
{
	FPrimaryAssetId Result;
	if (DefaultGameplayExperience.IsNull() || !bEnableGameplayExperience)
	{
		return Result;
	}

	Result = UAssetManager::Get().GetPrimaryAssetIdForPath(DefaultGameplayExperience.ToSoftObjectPath());

	if (!Result.IsValid())
	{
		UE_LOG(LogModularGameplayExperiences,
		       Error,
		       TEXT("%s.DefaultGameplayExperience is %s but that failed to resolve into an asset ID (you might need to add a path to the Asset Rules in your game feature plugin or project settings"),
		       *GetPathNameSafe(this),
		       *DefaultGameplayExperience.ToString());
	}

	return Result;
}
