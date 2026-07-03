// Copyright Chronicler.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FModularGameplayDataModule final : public IModuleInterface
{
public:

	/** IModuleInterface 接口实现。 */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
