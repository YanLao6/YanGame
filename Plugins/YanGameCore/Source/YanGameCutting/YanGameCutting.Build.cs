using UnrealBuildTool;

public class YanGameCutting : ModuleRules
{
	public YanGameCutting(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"GameplayAbilities",
				"ModularGameplayAbilities",
				"AngelscriptGAS",
				"PlanarCut",              // FPlanarCells, CutWithPlanarCells, BoxProjectUVs
				"GeometryCollectionEngine", // UGeometryCollectionComponent, FGeometryCollectionEdit, FGeometryCollectionEngineConversion
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Chaos", // FGeometryCollection, FTransformCollection group names
			}
		);
	}
}
