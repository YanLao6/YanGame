// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularGameplayAbilities.h"

#define LOCTEXT_NAMESPACE "FModularGameplayAbilitiesModule"

void FModularGameplayAbilitiesModule::StartupModule()
{
	// 模块载入内存后执行；具体时机见 .uplugin 中本 Module 的配置。
}

void FModularGameplayAbilitiesModule::ShutdownModule()
{
	// 支持热重载的模块在卸载前会调用此处做清理。
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FModularGameplayAbilitiesModule, ModularGameplayAbilities)
