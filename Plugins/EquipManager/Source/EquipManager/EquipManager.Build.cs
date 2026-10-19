// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EquipManager : ModuleRules
{
	public EquipManager(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ModularGameplay",
				"NetCore",       // FFastArraySerializer、FInventoryList 等网络复制
				"GameplayTags",   // FGameplayTag、FNativeGameplayTag、StatTag 相关
				"ModularGameplayAbilities", // UModularGameplayAbility 基类与 FModularAbilitySet 出现在公开头中
				"GameplayAbilities",
				"GameplayTasks",  // UGameplayTask/UAbilityTask 基类符号（AbilityTask_* 交互任务直接继承并重写）
				"AngelscriptGAS", // UAngelscriptGASAbility 基类 vtable 符号（GameplayAbility_Interact 继承链的根，K2_*GameplayCue_* 实现在此模块）
				"GameFeatures",
				"DataRegistry",   // FDataRegistryId 出现在 ItemDisplayTypes.h 的 UPROPERTY 上，下游模块需能解析
				"DeveloperSettings", // UDeveloperSettings 作为 UEquipManagerSettings 的基类出现在公开头中
				"ModularGameplayData", // UPawnDataFragment 作为公开头中的基类，下游模块需能解析
				"UMG",            // FInteractionOption 引用 UUserWidget；亦是 UCommonUserWidget 的基类
				"CommonUI",       // UCommonUserWidget 作为 UReticleWidgetBase 的基类出现在公开头中
				"SlateCore"       // FSlateBrush 出现在 InventoryFragment_QuickBarIcon 的 UPROPERTY 上
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Projects",                      // IPluginManager，定位插件目录以注册 Tag 源搜索路径
				"Slate",
				"GameplayMessageRuntime",
				"Niagara",
				"ModalCamera",                   // UModalCameraComponent::FindCameraComponent, GetBlendInfo
				"ModularGameplayExperiences",    // FGameplayTagStackContainer (StatTags 网络复制)
				"ModularGameplayUI",             // GA_Interact 对接 IndicatorSystem（UIndicatorManagerComponent / UIndicatorDescriptor）
				"AIModule",                      // GA_RangedWeapon 解算瞄准起点时区分 AAIController 与玩家控制器
				"Mover",                         // 散布计算从 UCharacterMoverComponent 读取蹲伏与滞空状态
				"ModularTeams",                  // WeaponStateComponent 依队伍关系判定命中是否算「成功」
				// ... add private dependencies that you statically link with here ...
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		// 按 Target 的 Iris 开关决定是否链接 IrisCore 并定义 UE_WITH_IRIS，
		// 避免在关闭 Iris 的配置下因硬依赖而编译失败
		SetupIrisSupport(Target);
	}
}
