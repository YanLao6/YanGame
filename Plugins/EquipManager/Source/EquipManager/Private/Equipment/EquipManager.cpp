// EquipManager 模块实现。

#include "Equipment/EquipManager.h"

#define LOCTEXT_NAMESPACE "FEquipManagerModule"

//~Begin IModuleInterface Interface
void FEquipManagerModule::StartupModule()
{
	// 当前模块暂无额外初始化逻辑，保留该入口以便后续扩展。
}

void FEquipManagerModule::ShutdownModule()
{
	// 当前模块暂无额外清理逻辑，保留该入口以便后续扩展。
}
//~End IModuleInterface Interface

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEquipManagerModule, EquipManager)
