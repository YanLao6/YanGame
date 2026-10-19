using UnrealBuildTool;

public class YanGameEffects : ModuleRules
{
	public YanGameEffects(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"GameplayTags",
				// UNiagaraSystem 与 UNiagaraComponent 出现在公开接口的资源列表中
				"Niagara",
				// UGameplayCueNotify_Static 作为 Cue 桥接类的基类
				"GameplayAbilities",
				// UControllerComponent 作为伤害飘字组件的基类
				"ModularGameplay"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				// UPhysicalMaterial
				"PhysicsCore",
				// UPhysicalMaterialWithTags 提供表面标签，是本项目既有的表面语义来源
				"ModularGameplayData"
			}
		);
	}
}
