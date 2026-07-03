// Copyright Chronicler.

using UnrealBuildTool;

public class ModularGameplayAbilities : ModuleRules
{
	public ModularGameplayAbilities(ReadOnlyTargetRules Target) : base(Target)
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
			new string[]
			{
				"Core",
				"GameFeatures",
				"GameplayAbilities",
				"ModalCamera",
				"ModularGameplay",
				"ModularGameplayExperiences",
				"CommonGame",
				"AngelscriptGAS" // 仅修改 UModularAbilitySystemComponent, UModularGameplayAbility 父类,不需要时可以改回去
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"EnhancedInput",
				"GameplayMessageRuntime",
				"GameplayTags",
				"GameplayTasks",
				"ModularGameplayActors",
				"ModularGameplayData",
				"ModularGameplayExperiences",
				"NetCore",
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