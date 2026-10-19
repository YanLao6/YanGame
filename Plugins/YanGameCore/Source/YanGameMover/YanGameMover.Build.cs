using UnrealBuildTool;

public class YanGameMover : ModuleRules
{
	public YanGameMover(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"Engine",
				"Mover",
				"ChaosMover",
				"AngelscriptGAS",
				"MoverExamples",
				"GameplayTags",
				"ModularGameplay",
				"ModularGameplayAbilities",
				"YanGameAbility",
				"GameplayAbilities",
				"GameplayTags",
				"UMG",
				"ModularGameplayUI"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"ModularGameplayData",
				"GameFeatures",
				"NetCore",
				"ModularGameplayExperiences",
				"GameplayTasks"
			}
		);

		// 编辑器内把 Pawn 模板上的 Mover 配置锁成只读，需要 PropertyEditor 的 EditConst 策略钩子
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("PropertyEditor");
		}

		SetupModulePhysicsSupport(Target);
	}
}