// Copyright Chronicler.

using UnrealBuildTool;

public class ModularGameplayExperiences : ModuleRules
{
	public ModularGameplayExperiences(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
			}
		);


		PrivateIncludePaths.AddRange(
			new string[]
			{
			}
		);


		PublicDependencyModuleNames.AddRange(
			new []
			{
				"CommonGame",
				"CommonInput",
				"CommonLoadingScreen",
				"CommonUser",
				"Core",
				"GameFeatures",
				"GameplayTags",
				"ModularGameplay",
				"ModularGameplayActors",
				"ModularGameplayData",
				"NetCore",
				"UMG"
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new []
			{
				"CoreUObject",
				"Engine",
				"EngineSettings",
				"EnhancedInput",
				"GameplayAbilities",
				"GameplayTags",
				"GameplayMessageRuntime",
				"Slate",
				"SlateCore"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}