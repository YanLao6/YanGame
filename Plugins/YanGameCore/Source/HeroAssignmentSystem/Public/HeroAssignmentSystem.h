#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

HEROASSIGNMENTSYSTEM_API DECLARE_LOG_CATEGORY_EXTERN(LogHeroAssignment, Log, All);

class FHeroAssignmentSystemModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
