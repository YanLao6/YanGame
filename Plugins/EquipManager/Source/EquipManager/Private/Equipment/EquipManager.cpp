// EquipManager 模块实现。

#include "Equipment/EquipManager.h"

#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FEquipManagerModule"

//~Begin IModuleInterface Interface
void FEquipManagerModule::StartupModule()
{
	// 插件 Config 目录不在引擎默认的 Tag 搜索范围内，需在此注册后 EquipManagerTag.ini 才会成为可用的 Tag 源。
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("EquipManager"));
	if (ensure(Plugin.IsValid()))
	{
		UGameplayTagsManager::Get().AddTagIniSearchPath(Plugin->GetBaseDir() / TEXT("Config"));
	}
}

void FEquipManagerModule::ShutdownModule()
{
	// 当前模块暂无额外清理逻辑，保留该入口以便后续扩展。
}
//~End IModuleInterface Interface

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEquipManagerModule, EquipManager)
