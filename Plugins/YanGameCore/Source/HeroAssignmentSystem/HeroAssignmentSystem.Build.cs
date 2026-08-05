using UnrealBuildTool;

public class HeroAssignmentSystem : ModuleRules
{
	public HeroAssignmentSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ModularGameplay",
				"ModularGameplayData",
				"ModularGameplayExperiences"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CommonUser",
				"CoreUObject",
				"Engine",
				"NetCore"
			}
		);
	}
}
