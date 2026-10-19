using UnrealBuildTool;

public class ColorTransferRuntime : ModuleRules
{
	public ColorTransferRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Mover",
				"ChaosMover",
				"GameplayAbilities",
				"GameplayTags",
				"EquipManager",               // IInteractableTarget / UGameplayAbility_Interact 作为公开头中的基类
				"UIExtension",                // UAbility_ColorInteract 公开头中的 FUIExtensionHandle 成员
				"HeroAssignmentSystem",     // UHeroAssignmentComponent 作为公开头中的基类
				"AngelscriptGAS",             // UModularGameplayAbility 的基类 UAngelscriptGASAbility
				"ModularGameplay",            // UPlayerStateComponent / UPawnComponent 作为公开头中的基类
				"ModularGameplayUI",           
				"ModularGameplayAbilities",   // UModularGameplayAbility 作为公开头中的基类
				"ModularGameplayExperiences", // UModularPlayerSpawningComponent 作为公开头中的基类
				"YanGameAbility",             // UYanAttributeSet / UYanGameplayAbility 作为公开头中的基类
				"YanGameMover",               // AYanAttractionActor 作为公开头中的基类
				"YanGameUI"                   // UYanUserWidget 作为公开头中的基类
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"NetCore",
				"DeveloperSettings", // UColorTransferSettings
				"GameplayTasks", // 蓄力技能里的 AbilityTask
				"InputCore",     // 吸引球读取鼠标左右键
				"YanGameSession", // 切关时转交大厅选队名单
				"ModularTeams",
				"ModularGameplayData", // 角色外观部件，颜料层要铺在部件网格上
				"Niagara",
				"YanGameplay",         // GameplayEvent.Death 与死亡技能
				"UMG",                 // 颜料构成条
				"SlateCore",
				"CommonUI",
				"CommonGame"           // 颜料构成条等待本地 PlayerState 就绪后绑定属性委托
			}
		);
	}
}
