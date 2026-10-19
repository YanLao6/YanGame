using UnrealBuildTool;

public class ShinjukuShowdownRuntime : ModuleRules
{
	public ShinjukuShowdownRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayAbilities",
				"GameplayTags",
				"AngelscriptGAS",           // UModularGameplayAbility 的基类 UAngelscriptGASAbility
				"ModularGameplayAbilities", // UModularGameplayAbility 作为公开头中的基类
				"EquipManager"              // UGameplayAbility_RangedWeapon 作为公开头中的基类
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"NetCore",      // FVector_NetQuantize 的序列化实现在 NetCore，import lib 不随 Engine 传递
				"GameplayTasks",// UAbilityTask_WaitInputRelease
				"Mover",
				"ChaosMover",
				"YanGameMover", // UYanMoverAngelscriptLibrary 的速度注入与冲量接口
				"ModularGameplayExperiences" // UVerbMessageHelpers 解析 ASC
			}
		);

		SetupModulePhysicsSupport(Target);
	}
}
