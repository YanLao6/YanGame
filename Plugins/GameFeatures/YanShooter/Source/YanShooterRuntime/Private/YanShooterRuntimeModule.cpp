#include "YanShooterRuntimeModule.h"

#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "YanShooterRuntime"

namespace
{
	// 插件 Config 目录不在引擎默认的 Tag 搜索范围内，需注册后其 Tag ini 才会成为可用的 Tag 源。
	void RegisterPluginTagSource(const TCHAR* PluginName)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
		if (ensure(Plugin.IsValid()))
		{
			UGameplayTagsManager::Get().AddTagIniSearchPath(Plugin->GetBaseDir() / TEXT("Config"));
		}
	}
}

void FYanShooterRuntimeModule::StartupModule()
{
	RegisterPluginTagSource(TEXT("YanShooter"));

	// YanShooterContent 是纯内容插件、没有自己的模块，其 Tag 源一并在此注册。
	RegisterPluginTagSource(TEXT("YanShooterContent"));
}

void FYanShooterRuntimeModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FYanShooterRuntimeModule, YanShooterRuntime)
