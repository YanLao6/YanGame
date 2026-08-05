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
				"IrisCore", 
				"ModularGameplayAbilities", // FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject（Public 以便正确链接）
				"GameplayAbilities",
				"GameplayTasks",  // UGameplayTask/UAbilityTask 基类符号（AbilityTask_* 交互任务直接继承并重写）
				"AngelscriptGAS", // UAngelscriptGASAbility 基类 vtable 符号（GameplayAbility_Interact 继承链的根，K2_*GameplayCue_* 实现在此模块）
				"GameFeatures",
				"DataRegistry",   // FDataRegistryId 出现在 ItemDisplayTypes.h 的 UPROPERTY 上，下游模块需能解析
				"ModularGameplayData" // UPawnDataFragment 作为公开头中的基类，下游模块需能解析
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
				"SlateCore",
				"UMG",                           // FInteractionOption 引用 UUserWidget（TSoftClassPtr）
				"GameplayMessageRuntime",
				"Niagara",
				"ModalCamera",                   // UModalCameraComponent::FindCameraComponent, GetBlendInfo
				"ModularGameplayExperiences",    // FGameplayTagStackContainer (StatTags 网络复制)
				"ModularGameplayUI",             // GA_Interact 对接 IndicatorSystem（UIndicatorManagerComponent / UIndicatorDescriptor）
				// ... add private dependencies that you statically link with here ...
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
