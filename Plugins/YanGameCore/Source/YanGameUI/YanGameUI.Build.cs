using UnrealBuildTool;

public class YanGameUI : ModuleRules
{
	public YanGameUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ModularGameplay",
				"CommonLoadingScreen",
				"CommonUI",
				"ControlFlows",
				"CommonUser",
				"ModularGameplayExperiences",
				"UMG",
				"ModularGameplayUI",
				// 设置界面基类与设置树
				"GameSettings",
				"YanGameSetting",
				"InputCore",
				"EnhancedInput"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"ApplicationCore",
				"CommonGame",
				"GameplayTags"
			}
		);
	}
}
