// Copyright Chronicler.

#include "ModularGameplayExperiences.h"

#include "ModularGameplayExperiencesLogs.h"
#include "Engine/AssetManagerSettings.h"
#include "GameMode/ModularExperienceDefinition.h"

#define LOCTEXT_NAMESPACE "FModularGameplayExperiencesModule"

void FModularGameplayExperiencesModule::StartupModule()
{

}

void FModularGameplayExperiencesModule::ShutdownModule()
{
	// 关闭阶段清理模块；支持动态重载的模块会在卸载前调用此处。
}

#undef LOCTEXT_NAMESPACE

DEFINE_LOG_CATEGORY(LogModularGameplayExperiences);

IMPLEMENT_MODULE(FModularGameplayExperiencesModule, ModularGameplayExperiences)
