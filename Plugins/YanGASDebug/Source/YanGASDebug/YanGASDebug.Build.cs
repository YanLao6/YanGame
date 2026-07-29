// Copyright YanGame.

using UnrealBuildTool;

public class YanGASDebug : ModuleRules
{
	public YanGASDebug(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"GameplayAbilities",
				"GameplayTags"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				// 提供 UModularAbilitySystemComponent，用于走复制安全的动态 Tag GE 路径。
				"ModularGameplayAbilities"
			}
		);
	}
}
