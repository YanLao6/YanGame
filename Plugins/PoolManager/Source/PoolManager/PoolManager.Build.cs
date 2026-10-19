// Copyright (c) Yevhenii Selivanov.

using UnrealBuildTool;

public class PoolManager : ModuleRules
{
	public PoolManager(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppCompileWarningSettings.NonInlinedGenCppWarningLevel = WarningLevel.Error;

		PublicDependencyModuleNames.AddRange(new[]
			{
				"Core"
				, "DeveloperSettings" // 用于实现 UPoolManagerSettings
				, "GameFeatures" // 用于继承 IGameFeatureStateChangeObserver
			}
		);

		PrivateDependencyModuleNames.AddRange(new[]
			{
				"CoreUObject", "Engine", "Slate", "SlateCore" // 核心模块
				, "UMG" // 用于实现 UPoolFactory_UserWidget
			}
		);

		if (Target.bBuildEditor)
		{
			// 引入本运行时模块所使用的编辑器模块
			PrivateDependencyModuleNames.AddRange(new[]
				{
					"UnrealEd" // GEditor
				}
			);
		}
	}
}