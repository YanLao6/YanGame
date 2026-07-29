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
				"AsyncMixin",
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
				"InputCore",     // FKey::IsValid / GetDisplayName（按键提示查询）
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
