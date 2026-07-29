using UnrealBuildTool;

public class YanGameMover : ModuleRules
{
	public YanGameMover(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"Engine",
				"Mover",
				"ChaosMover",
				"AngelscriptGAS",
				"MoverExamples",
				"GameplayTags",
				"ModularGameplay",
				"ModularGameplayAbilities",
				"YanGameAbility",
				"GameplayAbilities",
				"GameplayTags"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"ModularGameplayData",
				"GameFeatures",
				"NetCore",
				"ModularGameplayExperiences",
				"GameplayTasks"
			}
		);
	}
}