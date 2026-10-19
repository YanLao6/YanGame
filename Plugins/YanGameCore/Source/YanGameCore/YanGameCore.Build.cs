// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class YanGameCore : ModuleRules
{
	public YanGameCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CommonGame",
				"ModularGameplayExperiences",
				"ModularGameplayData",
				"EnhancedInput",
				"YanGameSetting",
				// UDeveloperSettings 作为 UYanDeveloperSettings 的基类出现在公开头中，
				// 其构造析构与整条 vtable 的符号由该模块提供
				"DeveloperSettings"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"GameplayTags",
				"Projects",
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
