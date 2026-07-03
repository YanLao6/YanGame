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
				"ModularGameplayUI"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"CommonGame",
				"GameplayTags"
			}
		);
	}
}