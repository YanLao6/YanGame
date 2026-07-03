using UnrealBuildTool;

public class YanGameplay : ModuleRules
{
	public YanGameplay(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ModularGameplayExperiences",
				"ModularGameplayActors",
				"ModularGameplay",
				"GameplayTags",
				"GameplayCameras",
				"CommonGame",
				"ModularGameplayAbilities",
				"GameplayAbilities"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"ModularGameplayAbilities",
				"Mover",
				"EnhancedInput",
				"ModularGameplayData",
				"YanGameUI",
				"YanGameMover"
			}
		);
	}
}