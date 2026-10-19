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

		// 按 Target 的 Iris 开关决定是否链接 IrisCore 并定义 UE_WITH_IRIS，
		// FModularGameplayEffectContext 需要据此注册转发用的 NetSerializer
		SetupIrisSupport(Target);
	}
}