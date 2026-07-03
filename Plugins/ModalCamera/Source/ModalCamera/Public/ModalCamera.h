// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FModalCameraModule final : public IModuleInterface
{
public:

	/** IModuleInterface：模块加载到内存后调用。 */
	virtual void StartupModule() override;
	/** IModuleInterface：模块卸载前调用（支持动态重载时会在 unload 前触发）。 */
	virtual void ShutdownModule() override;
};
