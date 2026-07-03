// Copyright Epic Games, Inc. All Rights Reserved.

#include "YanGameCore.h"
#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FYanGameCoreModule"

void FYanGameCoreModule::StartupModule()
{
	// 通过 IPluginManager 动态定位插件根目录，无论插件放在项目还是引擎 Plugins 目录都能正确找到
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("YanGameCore"));
	if (ensure(Plugin.IsValid()))
	{
		UGameplayTagsManager::Get().AddTagIniSearchPath(Plugin->GetBaseDir() / TEXT("Config/Tags"));
	}
}

void FYanGameCoreModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FYanGameCoreModule, YanGameCore)