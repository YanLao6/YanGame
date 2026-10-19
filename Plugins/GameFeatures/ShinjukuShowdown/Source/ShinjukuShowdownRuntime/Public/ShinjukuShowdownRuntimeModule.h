#pragma once

#include "Modules/ModuleManager.h"

class FShinjukuShowdownRuntimeModule : public IModuleInterface
{
public:
	//~Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~End IModuleInterface
};
