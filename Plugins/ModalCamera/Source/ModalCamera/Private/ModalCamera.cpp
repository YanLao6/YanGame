// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModalCamera.h"

#define LOCTEXT_NAMESPACE "FModalCameraModule"

void FModalCameraModule::StartupModule()
{
	// 模块载入内存后执行；具体时机由 .uplugin 中本 Module 的配置决定
}

void FModalCameraModule::ShutdownModule()
{
	// 卸载前调用，用于清理；支持动态重载的模块会在 unload 前先走此处
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FModalCameraModule, ModalCamera)
