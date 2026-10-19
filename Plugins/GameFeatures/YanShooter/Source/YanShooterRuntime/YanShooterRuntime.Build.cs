using UnrealBuildTool;

public class YanShooterRuntime : ModuleRules
{
	public YanShooterRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"ModularGameplay",
				"ModularGameplayActors",     // AModularAIController 作为公开头中的基类
				"CommonGame",
				"ModularGameplayExperiences" // UModularPlayerSpawningComponent 作为公开头中的基类
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"GameplayTags",
				"Projects",       // IPluginManager，定位插件目录以注册 Tag 源搜索路径
				"GameplayTasks",
				"GameplayAbilities",
				"GameplayMessageRuntime",
				"CommonUI",
				"UMG",
				"DataRegistry",
				"AsyncMixin",
				"EnhancedInput",
				"GameSubtitles",
				"DeveloperSettings",
				"AIModule",
				"EquipManager",   // 拾取物与武器实例
				"ModularTeams",   // 出生点选择与击杀结算依队伍关系判定
				"ModularGameplayUI",
				"ModularGameplayAbilities", // 瞄准辅助以 Status.Death 系列 Tag 判定目标是否已死亡
				"CommonInput",    // 瞄准辅助按输入方式区分视场缩放
				"YanGameCore",    // UYanLocalPlayer
				"YanGameSetting", // 输入修饰器基类、灵敏度查表与共享设置
				"YanGameplay"     // AYanPlayerState 的队伍编号
			}
		);
	}
}
