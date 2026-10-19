using UnrealBuildTool;

public class YanGameSession : ModuleRules
{
	public YanGameSession(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"UMG",
				"CommonUser",
				"ModularGameplay",
				"ModularGameplayExperiences",
				"GameplayTags",       // 回放子系统以平台特性 Tag 判定是否支持回放
				"NetworkReplayStreaming" // FNetworkReplayStreamInfo 等类型出现在公开头中
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"DeveloperSettings",  // UYanSessionSettings
				"ModularTeams",       // 大厅队伍的展示资产
				"NetCore",            // 大厅组件的属性复制
				"CoreOnline",         // 大厅名单以 FUniqueNetIdRepl 作键
				"CommonGame",
				// Steam 好友邀请面板经 IOnlineExternalUI 弹出，Utils 提供按 World 解析子系统的入口
				"OnlineSubsystem",
				"OnlineSubsystemUtils",
				"CommonUI",           // 读取平台特性集合以判定回放支持
				"YanGameCore",        // UYanLocalPlayer
				"YanGameSetting",     // UYanSettingsLocal 的回放留存设置
				"YanGameUI"           // UYanLoadingScreenSubsystem
			}
		);
	}
}
