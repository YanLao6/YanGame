using UnrealBuildTool;

public class YanGameAbility : ModuleRules
{
    public YanGameAbility(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "ModularGameplayAbilities",
                "GameplayAbilities",
                // FNativeGameplayTag 与 FGameplayTagContainer 的实现不随 GameplayAbilities 传递
                "GameplayTags",
                "AngelscriptGAS",
                "EquipManager"  // UGameplayAbility_RangedWeapon 作为公开头中的基类，下游模块需能解析
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "GeometryCollectionEngine", // UGeometryCollectionComponent，撞击处施加断裂应力
                "NetCore",      // FVector_NetQuantize 的序列化实现在 NetCore，import lib 不随 Engine 传递
                "Slate",
                "SlateCore",
                "ModularGameplayExperiences",
                "ModularTeams",  // 伤害结算查询队伍关系判定友伤
                "PhysicsCore"    // UPhysicalMaterial，伤害的物理材质衰减
            }
        );
    }
}