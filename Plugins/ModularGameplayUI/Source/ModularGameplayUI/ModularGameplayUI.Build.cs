// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ModularGameplayUI : ModuleRules
{
	public ModularGameplayUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"CommonLoadingScreen",
				"Core",
				"EnhancedInput",
				"GameplayTags",
				"ModularGameplayData",
				"ModularGameplayExperiences",
				"UIExtension"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CommonGame",
				"CommonInput",
				"DeveloperSettings",
				"CommonUI",
				"CommonUser",
				"ControlFlows",
				"CoreUObject",
				"Engine",
				"GameFeatures",
				"GameplayAbilities",
				"ModularGameplay",
				"ModularGameplayAbilities",
				"ModularGameplayActors",
				"Slate",
				"SlateCore",
				"UIExtension",
				"UMG",
				"WebRTC",
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
