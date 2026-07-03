// Copyright Chronicler.

#include "ModularGameplayData.h"

#include "ModularGameplayDataLogs.h"

#define LOCTEXT_NAMESPACE "FModularGameplayDataModule"

void FModularGameplayDataModule::StartupModule()
{
	// 模块载入内存后执行；具体时机由各模块的 .uplugin 配置决定。
}

void FModularGameplayDataModule::ShutdownModule()
{
	// 关闭时可能调用以清理模块；支持热重载时在卸载前会先调用本函数。
}

#undef LOCTEXT_NAMESPACE

DEFINE_LOG_CATEGORY(LogModularGameplayData);

IMPLEMENT_MODULE(FModularGameplayDataModule, ModularGameplayData)
