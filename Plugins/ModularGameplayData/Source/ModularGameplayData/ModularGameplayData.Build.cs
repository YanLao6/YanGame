// Copyright Chronicler.

using UnrealBuildTool;

public class ModularGameplayData : ModuleRules
{
	public ModularGameplayData(ReadOnlyTargetRules Target) : base(Target)
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
				"CommonInput",
				"EnhancedInput",
				"GameFeatures",
				"ModularGameplay",
				"ModularGameplayActors",
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"PhysicsCore",
				"Slate",
				"SlateCore",
				// 外观作弊按短名解析部件蓝图类
				"AssetRegistry",
				// FastArraySerializer 增量复制部件列表
				"NetCore",
				// UDeveloperSettingsBackedByCVars 作为外观调试设置的基类
				"DeveloperSettings",
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
