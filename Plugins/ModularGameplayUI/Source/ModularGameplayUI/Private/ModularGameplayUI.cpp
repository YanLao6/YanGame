// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularGameplayUI.h"

#define LOCTEXT_NAMESPACE "FModularGameplayUIModule"

void FModularGameplayUIModule::StartupModule()
{
	// 模块载入内存后执行；具体时机由 .uplugin 中该模块配置决定。
}

void FModularGameplayUIModule::ShutdownModule()
{
	// 关闭流程中可能调用以清理模块；支持动态重载的模块会在卸载前进入此函数。
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FModularGameplayUIModule, ModularGameplayUI)