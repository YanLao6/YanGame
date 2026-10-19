#pragma once

#include "Modules/ModuleManager.h"

class FYanShooterRuntimeModule : public IModuleInterface
{
public:
	//~Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~End IModuleInterface
};
