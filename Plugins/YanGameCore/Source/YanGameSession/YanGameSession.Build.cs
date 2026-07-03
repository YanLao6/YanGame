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
				"ModularGameplayExperiences"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"CommonGame"
			}
		);
	}
}
