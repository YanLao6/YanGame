// Copyright (c) Yevhenii Selivanov.

#include "PoolManagerEditorModule.h"

// UE
#include "Modules/ModuleManager.h"

void FPoolManagerEditorModule::StartupModule()
{
	// 这段代码在模块被载入内存后执行；具体时机由 .uplugin 文件中各模块的配置决定
}

void FPoolManagerEditorModule::ShutdownModule()
{
	// 该函数可能在关闭过程中被调用，用于清理模块。对于支持动态重载的模块，
	// 会在卸载模块之前调用本函数。
}

IMPLEMENT_MODULE(FPoolManagerEditorModule, PoolManagerEditor)
