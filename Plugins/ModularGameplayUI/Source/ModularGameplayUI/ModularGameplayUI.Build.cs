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
				"UIExtension",
				// UCommonListView 作为 UModularListView 的基类出现在公开头中
				"CommonUI",
				// UUserWidget / IWidgetCompilerLog 出现在公开头与其编辑器校验中
				"UMG",
				// UCommonGameDialog 与 UCommonMessagingSubsystem 作为基类出现在公开头中
				"CommonGame"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AsyncMixin",
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
				"UMG",
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
