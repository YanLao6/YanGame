// Copyright (c) Yevhenii Selivanov.

using UnrealBuildTool;

public class PoolManagerEditor : ModuleRules
{
    public PoolManagerEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppCompileWarningSettings.NonInlinedGenCppWarningLevel = WarningLevel.Error;

        PublicDependencyModuleNames.AddRange(new[]
            {
                "Core"
                , "BlueprintGraph" // 用于实现 UK2Node_TakeFromPool
            }
        );

		PrivateDependencyModuleNames.AddRange(new[]
			{
				"CoreUObject", "Engine", "Slate", "SlateCore" // 核心模块
				, "UnrealEd"
				, "KismetCompiler"
				// 本插件自有模块
				, "PoolManager"
			}
		);
    }
}