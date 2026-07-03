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
				"ModularGameplayAbilities" // FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject（Public 以便正确链接）
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"GameplayMessageRuntime",
				"Niagara",
				"GameplayAbilities",
				"ModalCamera",                   // UModalCameraComponent::FindCameraComponent, GetBlendInfo
				"ModularGameplayData",           // UPhysicalMaterialWithTags, FModularAnimLayerSelectionSet
				"ModularGameplayExperiences",    // FGameplayTagStackContainer (StatTags 网络复制)
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
