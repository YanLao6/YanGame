// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameSubtitles : ModuleRules
{
	public GameSubtitles(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... 在此添加其他需要静态链接的 public 依赖 ...
				"Overlay",
                "UMG",
				"MediaAssets",
				"MediaUtils",
				"GameplayTags"
			}
		);

        PublicIncludePathModuleNames.AddRange(
            new string[] {
                "UMG",
            }
        );
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// ... 在此添加需要静态链接的 private 依赖 ...
			}
		);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... 在此添加本模块动态加载的模块 ...
			}
		);
	}
}
