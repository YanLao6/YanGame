#include "HeroAssignmentSystem.h"

DEFINE_LOG_CATEGORY(LogHeroAssignment);

#define LOCTEXT_NAMESPACE "FHeroAssignmentSystemModule"

void FHeroAssignmentSystemModule::StartupModule()
{
}

void FHeroAssignmentSystemModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHeroAssignmentSystemModule, HeroAssignmentSystem)
