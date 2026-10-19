using UnrealBuildTool;

public class ModularTeams : ModuleRules
{
	public ModularTeams(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// FGenericTeamId 与 IGenericTeamAgentInterface
				"AIModule",
				"GameplayTags",
				// UGameStateComponent
				"ModularGameplay",
				// FGameplayTagStackContainer、UModularExperienceComponent 与 AModularExperienceGameMode
				"ModularGameplayExperiences",
				// FGameplayTagStackContainer 依赖的 FFastArraySerializer
				"NetCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				// UAbilitySystemGlobals：判定无队伍归属的目标能否被伤害
				"GameplayAbilities",
				// UNiagaraComponent：队伍配色应用到特效
				"Niagara"
			}
		);
	}
}
