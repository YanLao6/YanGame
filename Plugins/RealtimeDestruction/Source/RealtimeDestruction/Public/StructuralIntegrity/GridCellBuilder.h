// Copyright (c) 2026 LazyDevelopers <lazydeveloper24@gmail.com>. All rights reserved.
// This plugin is distributed under the Fab Standard License.
//
// This product was independently developed by us while participating in the Epic Project, a developer-support
// program of the KRAFTON JUNGLE GameTech Lab. All rights, title, and interest in and to the product are exclusively
// vested in us. Krafton, Inc. was not involved in its development and distribution and disclaims all representations
// and warranties, express or implied, and assumes no responsibility or liability for any consequences arising from
// the use of this product.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "StructuralIntegrity/GridCellTypes.h"
#include "VectorTypes.h"  // UE::Geometry::FVector3d

class UBodySetup;
struct FKConvexElem;
struct FKBoxElem;
struct FKSphereElem;
struct FKSphylElem;

namespace UE { namespace Geometry { class FDynamicMesh3; } }

/**
 * 网格单元构建器（编辑器工具）。
 * 从 StaticMesh 或 DynamicMesh 构建网格单元布局。
 */
class REALTIMEDESTRUCTION_API FGridCellBuilder
{
public:
	/**
	 * 从静态网格构建网格单元布局。
	 *
	 * @param SourceMesh - 源网格
	 * @param MeshScale - 网格缩放（组件缩放）
	 * @param CellSize - 单元格大小（cm，世界空间）
	 * @param AnchorHeightThreshold - 锚点高度阈值（cm）
	 * @param OutLayout - 输出布局
	 * @return 构建是否成功
	 */
	static bool BuildFromStaticMesh(
		const UStaticMesh* SourceMesh,
		const FVector& MeshScale,
		const FVector& CellSize,
		float AnchorHeightThreshold,
		FGridCellLayout& OutLayout,
		TMap<int32, FSubCell>* OutSubCellStates = nullptr
		);

	/**
	 * 从动态网格构建网格单元布局。
	 *
	 * @param Mesh - 源动态网格
	 * @param CellSize - 单元格大小（cm）
	 * @param AnchorHeightThreshold - 锚点高度阈值（cm）
	 * @param OutLayout - 输出布局
	 * @return 构建是否成功
	 */
	static bool BuildFromDynamicMesh(
		const UE::Geometry::FDynamicMesh3& Mesh,
		const FVector& CellSize,
		float AnchorHeightThreshold,
		FGridCellLayout& OutLayout);

	static bool TriangleIntersectsAABB(
		const FVector& V0, const FVector& V1, const FVector& V2,
		const FVector& BoxMin, const FVector& BoxMax
	);

	/**
	* 如果子单元格与给定的三角形相交，则将其标记为活动状态。
	*
	* @param V0, V1, V2 - 三角形顶点（局部空间）
	* @param CellMin - 单元格最小角（局部空间）
	* @param CellSize - 单元格大小（局部空间）
	* @param OutSubCellState - 要更新的 SubCell 状态（活动状态的位设置为 1）
	*/

	static void MarkIntersectingSubCellsAlive(
		const FVector& V0, const FVector& V1, const FVector& V2,
		const FVector& CellMin, const FVector& CellSIze,
		FSubCell& OutSubCellState
	);

	static void SetAnchorsByFinitePlane(
		const FTransform& PlaneTransform,
		const FTransform& MeshTransform,
		FGridCellLayout& OutLayout,
		bool bIsEraser);

	static void SetAnchorsByFiniteBox(
		const FTransform& BoxTransform,
		const FVector& BoxExtent,
		const FTransform& MeshTransform,
		FGridCellLayout& OutLayout,
		bool bIsEraser);

	static void SetAnchorsByFiniteSphere(
		const FTransform& SphereTransform,
		float SphereRadius,
		const FTransform& MeshTransform,
		FGridCellLayout& OutLayout,
		bool bIsEraser);

	static void ClearAllAnchors(FGridCellLayout& OutLayout);

private:
	/**
	 * 从边界框计算网格尺寸。
	 */
	static void CalculateGridDimensions(
		const FBox& Bounds,
		const FVector& CellSize,
		FGridCellLayout& OutLayout);

	/**
	 * 将三角形分配给单元格。
	 */
	static void AssignTrianglesToCells(
		const UE::Geometry::FDynamicMesh3& Mesh,
		FGridCellLayout& OutLayout);

	/**
	 * 计算邻接关系（6个方向）。
	 */
	static void CalculateNeighbors(FGridCellLayout& OutLayout);

	/**
	 * 确定锚点单元格。
	 */
	static void DetermineAnchors(
		FGridCellLayout& OutLayout,
		float HeightThreshold);
	
	/**
	 * 体素化网格（填充内部单元格）- DynamicMesh 版本。
	 */
	static void VoxelizeMesh(
		const UE::Geometry::FDynamicMesh3& Mesh,
		FGridCellLayout& OutLayout);

	/**
	 * 使用所有碰撞类型（凸面体、长方体、球体、胶囊体）进行体素化。
	 */
	static void VoxelizeWithCollision(
		const UBodySetup* BodySetup,
		FGridCellLayout& OutLayout);

	/**
	 * 使用凸面碰撞进行体素化（兼容性）。
	 */
	static void VoxelizeWithConvex(
		const UBodySetup* BodySetup,
		FGridCellLayout& OutLayout);

	/** 使用 StaticMesh 渲染三角形进行体素化。 */
	static void VoxelizeWithTriangles(
		const UStaticMesh* SourceMesh,
		FGridCellLayout& OutLayout,
		TMap<int32, FSubCell>* OutSubCellStates);

	/** 体素化单个三角形（可选支持 SubCell）。 */
	static void VoxelizeTriangle(
		const FVector& V0,
		const FVector& V1,
		const FVector& V2,
		FGridCellLayout& OutLayout,
		TMap<int32, FSubCell>* OutSubCellStates);

	/** 从顶点/索引数组进行体素化（用于缓存数据）。 */
	static void VoxelizeFromArrays(
		const TArray<FVector>& Vertices,
		const TArray<uint32>& Indices,
		FGridCellLayout& OutLayout,
		TMap<int32, FSubCell>* OutSubCellStates);


	static void FillInsideVoxels(FGridCellLayout& OutLayout);

	/**
	 * 检查一个点是否在凸包内。
	 */
	static bool IsPointInsideConvex(
		const FKConvexElem& ConvexElem,
		const FVector& Point);

	/**
	 * 检查一个点是否在长方体内。
	 */
	static bool IsPointInsideBox(
		const FKBoxElem& BoxElem,
		const FVector& Point);

	/**
	 * 检查一个点是否在球体内。
	 */
	static bool IsPointInsideSphere(
		const FKSphereElem& SphereElem,
		const FVector& Point);

	/**
	 * 检查一个点是否在胶囊体内。
	 */
	static bool IsPointInsideCapsule(
		const FKSphylElem& CapsuleElem,
		const FVector& Point);

};
