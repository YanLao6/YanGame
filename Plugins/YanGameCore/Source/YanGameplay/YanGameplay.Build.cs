using UnrealBuildTool;

public class YanGameplay : ModuleRules
{
	public YanGameplay(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ModularGameplayExperiences",
				"ModularGameplayActors",
				"ModularGameplay",
				"GameplayTags",
				"GameplayCameras",
				"CommonGame",
				// FUIExtensionHandle 作为公开头中的观战界面挂载句柄
				"UIExtension",
				// TSubclassOf<UUserWidget> 需要 UUserWidget 的完整类型
				"UMG",
				"ModularGameplayAbilities",
				"GameplayAbilities",
				// UYanGameplayAbility 与 UYanImpulseSet 作为公开头中的基类与属性源
				"YanGameAbility",
				// UAngelscriptGASAbility 在技能基类链上，其虚函数实现须由本模块直接链接
				"AngelscriptGAS",
				"ModularTeams",
				// FGenericTeamId 与 IGenericTeamAgentInterface
				"AIModule"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"NetCore",
				"Slate",
				"SlateCore",
				"ModularGameplayAbilities",
				"GameplayMessageRuntime", // 生命组件在死亡时广播击杀消息
				"YanGameSession",         // 玩家控制器经回放子系统发起客户端录制
				"Mover",
				"EnhancedInput",
				"ModularGameplayData",
				"HeroAssignmentSystem",
				"YanGameUI",
				"YanGameMover"
			}
		);
	}
}