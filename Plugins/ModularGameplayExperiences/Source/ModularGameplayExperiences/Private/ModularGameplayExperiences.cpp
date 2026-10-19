// Copyright Chronicler.

#include "ModularGameplayExperiences.h"

#include "ModularGameplayExperiencesLogs.h"
#include "ModularGameplayExperienceCheat.h"
#include "Engine/AssetManagerSettings.h"
#include "GameMode/ModularExperienceDefinition.h"

#define LOCTEXT_NAMESPACE "FModularGameplayExperiencesModule"

void FModularGameplayExperiencesModule::StartupModule()
{
#if !UE_BUILD_SHIPPING
	CheatManagerCreatedHandle = UCheatManager::RegisterForOnCheatManagerCreated(
		FOnCheatManagerCreated::FDelegate::CreateLambda([](UCheatManager* CheatManager)
		{
			if (CheatManager->FindCheatManagerExtension<UModularGameplayExperienceCheat>() == nullptr)
			{
				CheatManager->AddCheatManagerExtension(NewObject<UModularGameplayExperienceCheat>(CheatManager));
			}
		}));
#endif
}

void FModularGameplayExperiencesModule::ShutdownModule()
{
#if !UE_BUILD_SHIPPING
	UCheatManager::UnregisterFromOnCheatManagerCreated(CheatManagerCreatedHandle);
	CheatManagerCreatedHandle.Reset();
#endif
}

#undef LOCTEXT_NAMESPACE

DEFINE_LOG_CATEGORY(LogModularGameplayExperiences);

IMPLEMENT_MODULE(FModularGameplayExperiencesModule, ModularGameplayExperiences)
