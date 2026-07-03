// Copyright (c) 2026 LazyDevelopers <lazydeveloper24@gmail.com>. All rights reserved.
// This plugin is distributed under the Fab Standard License.
//
// This product was independently developed by us while participating in the Epic Project, a developer-support
// program of the KRAFTON JUNGLE GameTech Lab. All rights, title, and interest in and to the product are exclusively
// vested in us. Krafton, Inc. was not involved in its development and distribution and disclaims all representations
// and warranties, express or implied, and assumes no responsibility or liability for any consequences arising from
// the use of this product.

#include "StructuralIntegrity/GridCellBuilder.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/ConvexElem.h"

using namespace UE::Geometry;

//=============================================================================
// Public Methods
//=============================================================================

bool FGridCellBuilder::BuildFromStaticMesh(
	const UStaticMesh* SourceMesh,
	const FVector& MeshScale,
	const FVector& CellSize,
	float AnchorHeightThreshold,
	FGridCellLayout& OutLayout,
	TMap<int32, FSubCell>* OutSubCellStates)
{
	if (!SourceMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("FGridCellBuilder: SourceMesh is null"));
		return false;
	}

	OutLayout.Reset();
	OutLayout.CellSize = CellSize;

	// 1. 计算包围盒（保持局部空间）
	const FBox LocalBounds = SourceMesh->GetBoundingBox();
	if (!LocalBounds.IsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("FGridCellBuilder: Invalid mesh bounds"));
		return false;
	}

	// 存储缩放（用于碰撞检查）
	OutLayout.MeshScale = MeshScale;
	UE_LOG(LogTemp, Display, TEXT("BuildCell/BuildFromStaticMesh %s"), *OutLayout.MeshScale.ToCompactString());

	// 使用缩放后的大小计算网格维度（用于单元格计数）
	const FVector ScaledSize = LocalBounds.GetSize() * MeshScale;
	const FIntVector GridDimensions(
		FMath::Max(1, FMath::CeilToInt(ScaledSize.X / CellSize.X)),
		FMath::Max(1, FMath::CeilToInt(ScaledSize.Y / CellSize.Y)),
		FMath::Max(1, FMath::CeilToInt(ScaledSize.Z / CellSize.Z))
	);

	// 局部空间单元格大小（应用了逆缩放）
	const FVector LocalCellSize(
		CellSize.X / MeshScale.X,
		CellSize.Y / MeshScale.Y,
		CellSize.Z / MeshScale.Z
	);

	// 2. 配置网格（局部空间）
	OutLayout.GridOrigin = LocalBounds.Min;
	OutLayout.GridSize = GridDimensions;
	OutLayout.CellSize = LocalCellSize;  // 局部空间单元格大小

	const int32 TotalCells = OutLayout.GetTotalCellCount();

	UE_LOG(LogTemp, Log, TEXT("BuildFromStaticMesh: Scale=(%.2f, %.2f, %.2f), ScaledSize=(%.1f, %.1f, %.1f), WorldCellSize=%.1f, LocalCellSize=(%.2f, %.2f, %.2f), Grid=(%d,%d,%d), Total=%d"),
		MeshScale.X, MeshScale.Y, MeshScale.Z,
		ScaledSize.X, ScaledSize.Y, ScaledSize.Z,
		CellSize.X,
		LocalCellSize.X, LocalCellSize.Y, LocalCellSize.Z,
		OutLayout.GridSize.X, OutLayout.GridSize.Y, OutLayout.GridSize.Z,
		TotalCells);

	if (TotalCells <= 0 || TotalCells > 1000000)
	{
		UE_LOG(LogTemp, Warning, TEXT("FGridCellBuilder: Invalid cell count: %d"), TotalCells);
		return false;
	}

	// 3. 初始化位域（清零）
	OutLayout.InitializeBitfields();

	 // 4. 基于碰撞的体素化（优先级：Convex > Box > Sphere > Capsule > BoundingBox）
	 //UBodySetup* BodySetup = SourceMesh->GetBodySetup();
	 //if (BodySetup)
	 //{
	 //	VoxelizeWithCollision(BodySetup, OutLayout);
	 //}
	 //else
	 //{
	 //	// 如果没有 BodySetup，则用包围盒填充
	 //	UE_LOG(LogTemp, Warning, TEXT("FGridCellBuilder: No BodySetup, filling bounding box"));
	 //	for (int32 i = 0; i < TotalCells; i++)
	 //	{
	 //		OutLayout.SetCellExists(i, true);
	 //		OutLayout.RegisterValidCell(i);
	 //	}
	 //}
	
	VoxelizeWithTriangles(SourceMesh, OutLayout, OutSubCellStates);

	 FillInsideVoxels(OutLayout);

	// 6. 计算邻居
	CalculateNeighbors(OutLayout);

	// 7. 确定锚点
	DetermineAnchors(OutLayout, AnchorHeightThreshold);

	UE_LOG(LogTemp, Log, TEXT("FGridCellBuilder: Built grid %dx%dx%d, valid cells: %d"),
		OutLayout.GridSize.X, OutLayout.GridSize.Y, OutLayout.GridSize.Z,
		OutLayout.GetValidCellCount());

	return true;
}

bool FGridCellBuilder::BuildFromDynamicMesh(
	const FDynamicMesh3& Mesh,
	const FVector& CellSize,
	float AnchorHeightThreshold,
	FGridCellLayout& OutLayout)
{
	OutLayout.Reset();
	OutLayout.CellSize = CellSize;

	// 1. 计算包围盒
	FAxisAlignedBox3d Bounds = Mesh.GetBounds();
	if (Bounds.IsEmpty() || Bounds.Volume() <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("FGridCellBuilder: Invalid mesh bounds"));
		return false;
	}

	FBox UnrealBounds(
		FVector(Bounds.Min.X, Bounds.Min.Y, Bounds.Min.Z),
		FVector(Bounds.Max.X, Bounds.Max.Y, Bounds.Max.Z)
	);

	// 2. 计算网格维度
	CalculateGridDimensions(UnrealBounds, CellSize, OutLayout);

	const int32 TotalCells = OutLayout.GetTotalCellCount();
	if (TotalCells <= 0 || TotalCells > 1000000)  // 1,000,000 单元格限制
	{
		UE_LOG(LogTemp, Warning, TEXT("FGridCellBuilder: Invalid cell count: %d"), TotalCells);
		return false;
	}

	// 3. 初始化位域（清零）
	OutLayout.InitializeBitfields();

	// 4. 分配三角形
	AssignTrianglesToCells(Mesh, OutLayout);

	// 6. 计算邻居
	CalculateNeighbors(OutLayout);

	// 7. 确定锚点
	DetermineAnchors(OutLayout, AnchorHeightThreshold);

	UE_LOG(LogTemp, Log, TEXT("FGridCellBuilder: Built grid %dx%dx%d, valid cells: %d"),
		OutLayout.GridSize.X, OutLayout.GridSize.Y, OutLayout.GridSize.Z,
		OutLayout.GetValidCellCount());

	return true;
}

void FGridCellBuilder::MarkIntersectingSubCellsAlive(
	const FVector& V0, const FVector& V1, const FVector& V2,
	const FVector& CellMin, const FVector& CellSize,
	FSubCell& OutSubCellState)
{
	const FVector SubCellSize = CellSize / static_cast<float>(SUBCELL_DIVISION);

	for (int32 SubCellId = 0; SubCellId < SUBCELL_COUNT; ++SubCellId)
	{
		if (OutSubCellState.IsSubCellAlive(SubCellId))
		{
			continue;
		}

		const FIntVector SubCoord = SubCellIdToCoord(SubCellId);
		const FVector SubCellMin = CellMin + FVector(
			SubCoord.X * SubCellSize.X,
			SubCoord.Y * SubCellSize.Y,
			SubCoord.Z * SubCellSize.Z);

		const FVector SubCellMax = SubCellMin + SubCellSize;

		if (TriangleIntersectsAABB(V0, V1, V2, SubCellMin, SubCellMax))
		{
			OutSubCellState.Bits |= (1 << SubCellId);
		}
	}
}

void FGridCellBuilder::SetAnchorsByFinitePlane(
	const FTransform& PlaneTransform,
	const FTransform& MeshTransform,
	FGridCellLayout& OutLayout,
	bool bIsEraser)
{
	const int32 TotalCells = OutLayout.GetTotalCellCount();
	int32 AddedAnchors = 0;

	const float CubeExtent = 50.0f;

	for (int32 CellId = 0; CellId < TotalCells; ++CellId)
	{
		if (!OutLayout.GetCellExists(CellId))
		{
			continue;
		}

		FVector LocalPos = OutLayout.IdToLocalCenter(CellId);
		FVector WorldPos = MeshTransform.TransformPosition(LocalPos);

		FVector CubeSpacePos = PlaneTransform.InverseTransformPosition(WorldPos);

		bool bInsideY = FMath::Abs(CubeSpacePos.Y) <= CubeExtent;
		bool bInsideZ = FMath::Abs(CubeSpacePos.Z) <= CubeExtent;

		if (bInsideY && bInsideZ)
		{
			if (CubeSpacePos.X > 0.0f) 
			{
				if (bIsEraser)
				{
					if (OutLayout.GetCellExists(CellId))
					{
						OutLayout.SetCellIsAnchor(CellId, false);
					}
				}
				else
				{
					if (!OutLayout.GetCellIsAnchor(CellId))
					{
						OutLayout.SetCellIsAnchor(CellId, true);
						AddedAnchors++;
					}
				}
				
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("SetAnchorsByFinitePlane: %d cells marked as Anchor."), AddedAnchors);
}

void FGridCellBuilder::SetAnchorsByFiniteBox(
	const FTransform& BoxTransform,
	const FVector& BoxExtent,
	const FTransform& MeshTransform,
	FGridCellLayout& OutLayout,
	bool bIsEraser)
{
	const int32 TotalCells = OutLayout.GetTotalCellCount();
	int32 AddedAnchors = 0;
	int32 RemovedAnchors = 0;

	for (int32 CellId = 0; CellId < TotalCells; ++CellId)
	{
		if (!OutLayout.GetCellExists(CellId))
		{
			continue;
		}

		const FVector LocalPos = OutLayout.IdToLocalCenter(CellId);
		const FVector WorldPos = MeshTransform.TransformPosition(LocalPos);

		// 世界 -> 盒子局部空间（包括旋转/缩放）
		const FVector BoxSpacePos = BoxTransform.InverseTransformPosition(WorldPos);

		const bool bInside =
				FMath::Abs(BoxSpacePos.X) <= BoxExtent.X &&
				FMath::Abs(BoxSpacePos.Y) <= BoxExtent.Y &&
				FMath::Abs(BoxSpacePos.Z) <= BoxExtent.Z;

		if (!bInside)
		{
			continue;
		}

		if (bIsEraser)
		{
			if (OutLayout.GetCellIsAnchor(CellId))
			{
				OutLayout.SetCellIsAnchor(CellId, false);
				RemovedAnchors++;
			}
		}
		else
		{
			if (!OutLayout.GetCellIsAnchor(CellId))
			{
				OutLayout.SetCellIsAnchor(CellId, true);
				AddedAnchors++;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("SetAnchorsByFiniteBox: Added=%d, Removed=%d"), AddedAnchors, RemovedAnchors);
}

void FGridCellBuilder::SetAnchorsByFiniteSphere(
	const FTransform& SphereTransform,
	float SphereRadius,
	const FTransform& MeshTransform,
	FGridCellLayout& OutLayout,
	bool bIsEraser)
{
	const int32 TotalCells = OutLayout.GetTotalCellCount();
	int32 AddedAnchors = 0;
	int32 RemovedAnchors = 0;

	const float Radius = FMath::Max(0.0f, SphereRadius);
	const float RadiusSq = Radius * Radius;

	for (int32 CellId = 0; CellId < TotalCells; ++CellId)
	{
		if (!OutLayout.GetCellExists(CellId))
		{
			continue;
		}

		const FVector LocalPos = OutLayout.IdToLocalCenter(CellId);
		const FVector WorldPos = MeshTransform.TransformPosition(LocalPos);

		// 世界 -> 球体局部空间（逆变换包括缩放）
		// 在 SphereTransform 中使用非均匀缩放时，这在世界空间中会变成椭球体测试。
		const FVector SphereSpacePos = SphereTransform.InverseTransformPosition(WorldPos);

		const bool bInside = SphereSpacePos.SizeSquared() <= RadiusSq;
		if (!bInside)
		{
			continue;
		}

		if (bIsEraser)
		{
			if (OutLayout.GetCellIsAnchor(CellId))
			{
				OutLayout.SetCellIsAnchor(CellId, false);
				RemovedAnchors++;
			}
		}
		else
		{
			if (!OutLayout.GetCellIsAnchor(CellId))
			{
				OutLayout.SetCellIsAnchor(CellId, true);
				AddedAnchors++;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("SetAnchorsByFiniteSphere: Added=%d, Removed=%d, Radius=%.2f"), AddedAnchors,
	       RemovedAnchors, Radius);
}

void FGridCellBuilder::ClearAllAnchors(FGridCellLayout& OutLayout)
{
	const int32 TotalCells = OutLayout.GetTotalCellCount();
	int32 ClearedCount = 0;

	for (int32 i = 0; i < TotalCells; ++i)
	{
		if (OutLayout.GetCellExists(i) && OutLayout.GetCellIsAnchor(i))
		{
			OutLayout.SetCellIsAnchor(i, false);
			ClearedCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("ClearAllAnchors: %d cells reset."), ClearedCount);
}

//=============================================================================
// Private Methods
//=============================================================================

void FGridCellBuilder::CalculateGridDimensions(
	const FBox& Bounds,
	const FVector& CellSize,
	FGridCellLayout& OutLayout)
{
	OutLayout.GridOrigin = Bounds.Min;

	const FVector Size = Bounds.GetSize();

	OutLayout.GridSize = FIntVector(
		FMath::Max(1, FMath::CeilToInt(Size.X / CellSize.X)),
		FMath::Max(1, FMath::CeilToInt(Size.Y / CellSize.Y)),
		FMath::Max(1, FMath::CeilToInt(Size.Z / CellSize.Z))
	);
}

void FGridCellBuilder::AssignTrianglesToCells(
	const FDynamicMesh3& Mesh,
	FGridCellLayout& OutLayout)
{
	// 1. 首先体素化（注册有效单元格）
	VoxelizeMesh(Mesh, OutLayout);

	// 2. 将三角形分配给单元格（稀疏数组）
	for (int32 TriId : Mesh.TriangleIndicesItr())
	{
		const FIndex3i Tri = Mesh.GetTriangle(TriId);
		const FVector3d V0 = Mesh.GetVertex(Tri.A);
		const FVector3d V1 = Mesh.GetVertex(Tri.B);
		const FVector3d V2 = Mesh.GetVertex(Tri.C);
		const FVector3d TriCenter = (V0 + V1 + V2) / 3.0;

		const int32 X = FMath::Clamp(
			FMath::FloorToInt((TriCenter.X - OutLayout.GridOrigin.X) / OutLayout.CellSize.X),
			0, OutLayout.GridSize.X - 1);
		const int32 Y = FMath::Clamp(
			FMath::FloorToInt((TriCenter.Y - OutLayout.GridOrigin.Y) / OutLayout.CellSize.Y),
			0, OutLayout.GridSize.Y - 1);
		const int32 Z = FMath::Clamp(
			FMath::FloorToInt((TriCenter.Z - OutLayout.GridOrigin.Z) / OutLayout.CellSize.Z),
			0, OutLayout.GridSize.Z - 1);

		const int32 CellId = OutLayout.CoordToId(X, Y, Z);

		// 将三角形添加到稀疏数组
		FIntArray* Triangles = OutLayout.GetCellTrianglesMutable(CellId);
		if (Triangles)
		{
			Triangles->Add(TriId);
		}
	}
}

void FGridCellBuilder::VoxelizeMesh(
	const UE::Geometry::FDynamicMesh3& Mesh,
	FGridCellLayout& OutLayout)
{
	// DynamicMesh 版本 - 用包围盒填充（无凸包数据）
	const int32 TotalCells = OutLayout.GetTotalCellCount();
	for (int32 CellId = 0; CellId < TotalCells; CellId++)
	{
		OutLayout.SetCellExists(CellId, true);
		OutLayout.RegisterValidCell(CellId);
	}

	UE_LOG(LogTemp, Log, TEXT("VoxelizeMesh: Filled bounding box with %d cells"), TotalCells);
}

void FGridCellBuilder::VoxelizeWithCollision(
	const UBodySetup* BodySetup,
	FGridCellLayout& OutLayout)
{
	if (!BodySetup)
	{
		return;
	}

	const FKAggregateGeom& AggGeom = BodySetup->AggGeom;
	const int32 TotalCells = OutLayout.GetTotalCellCount();

	// 按碰撞类型检查计数
	const int32 NumConvex = AggGeom.ConvexElems.Num();
	const int32 NumBox = AggGeom.BoxElems.Num();
	const int32 NumSphere = AggGeom.SphereElems.Num();
	const int32 NumCapsule = AggGeom.SphylElems.Num();

	UE_LOG(LogTemp, Log, TEXT("VoxelizeWithCollision: Convex=%d, Box=%d, Sphere=%d, Capsule=%d"),
		NumConvex, NumBox, NumSphere, NumCapsule);

	// 凸包数据详细日志
	for (int32 i = 0; i < NumConvex; i++)
	{
		const FKConvexElem& Elem = AggGeom.ConvexElems[i];
		UE_LOG(LogTemp, Log, TEXT("  Convex[%d]: VertexData=%d, IndexData=%d"),
			i, Elem.VertexData.Num(), Elem.IndexData.Num());
	}

	// 盒子数据详细日志
	for (int32 i = 0; i < NumBox; i++)
	{
		const FKBoxElem& Elem = AggGeom.BoxElems[i];
		UE_LOG(LogTemp, Log, TEXT("  Box[%d]: Size=(%.1f, %.1f, %.1f), Center=(%.1f, %.1f, %.1f)"),
			i, Elem.X, Elem.Y, Elem.Z, Elem.Center.X, Elem.Center.Y, Elem.Center.Z);
	}

	// 如果没有碰撞，则用包围盒填充
	if (NumConvex == 0 && NumBox == 0 && NumSphere == 0 && NumCapsule == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("VoxelizeWithCollision: No collision elements, filling bounding box"));
		for (int32 i = 0; i < TotalCells; i++)
		{
			OutLayout.SetCellExists(i, true);
			OutLayout.RegisterValidCell(i);
		}
		return;
	}

	// 对每个单元格，测试其是否位于碰撞体内
	// （单元格中心在运行时计算；碰撞在局部空间中）
	for (int32 CellId = 0; CellId < TotalCells; CellId++)
	{
		const FVector CellCenterLocal = OutLayout.IdToLocalCenter(CellId);
		bool bCellExists = false;

		// 凸包检查
		for (const FKConvexElem& Elem : AggGeom.ConvexElems)
		{
			if (IsPointInsideConvex(Elem, CellCenterLocal))
			{
				bCellExists = true;
				break;
			}
		}

		// 盒子检查
		if (!bCellExists)
		{
			for (const FKBoxElem& Elem : AggGeom.BoxElems)
			{
				if (IsPointInsideBox(Elem, CellCenterLocal))
				{
					bCellExists = true;
					break;
				}
			}
		}

		// 球体检查
		if (!bCellExists)
		{
			for (const FKSphereElem& Elem : AggGeom.SphereElems)
			{
				if (IsPointInsideSphere(Elem, CellCenterLocal))
				{
					bCellExists = true;
					break;
				}
			}
		}

		// 胶囊体检查
		if (!bCellExists)
		{
			for (const FKSphylElem& Elem : AggGeom.SphylElems)
			{
				if (IsPointInsideCapsule(Elem, CellCenterLocal))
				{
					bCellExists = true;
					break;
				}
			}
		}

		// 注册有效单元格
		if (bCellExists)
		{
			OutLayout.SetCellExists(CellId, true);
			OutLayout.RegisterValidCell(CellId);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("VoxelizeWithCollision: Valid cells = %d / %d"),
		OutLayout.GetValidCellCount(), TotalCells);
}

void FGridCellBuilder::VoxelizeWithConvex(
	const UBodySetup* BodySetup,
	FGridCellLayout& OutLayout)
{
	// 保留以实现兼容性 - 委托给 VoxelizeWithCollision
	VoxelizeWithCollision(BodySetup, OutLayout);
}


void FGridCellBuilder::VoxelizeWithTriangles(
      const UStaticMesh* SourceMesh,
      FGridCellLayout& OutLayout,
	  TMap<int32, FSubCell>* OutSubCellStates)
{
    // 方法 0：首先尝试缓存的三角形数据（在打包构建中有效）
    if (OutLayout.HasCachedTriangleData())
  {
        UE_LOG(LogTemp, Log, TEXT("VoxelizeWithTriangles: Using CachedTriangleData (Vertices=%d, Triangles=%d)"),
            OutLayout.CachedVertices.Num(), OutLayout.CachedIndices.Num() / 3);

        VoxelizeFromArrays(OutLayout.CachedVertices, OutLayout.CachedIndices, OutLayout, OutSubCellStates);
        return;
    }

    // 方法 1：MeshDescription（原始方法 - 在编辑器中最准确）
      UStaticMeshDescription* StaticMeshDesc = const_cast<UStaticMesh*>(SourceMesh)->GetStaticMeshDescription(0);
      const FMeshDescription* MeshDesc = StaticMeshDesc ? &StaticMeshDesc->GetMeshDescription() : nullptr;

    if (MeshDesc)
      {
        FStaticMeshConstAttributes Attributes(*MeshDesc);
        TVertexAttributesConstRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();

        const int32 NumVerts = MeshDesc->Vertices().Num();
        const int32 NumTris = MeshDesc->Triangles().Num();

        if (NumVerts > 0 && NumTris > 0)
          {
            UE_LOG(LogTemp, Log, TEXT("VoxelizeWithTriangles: Using MeshDescription (Vertices=%d, Triangles=%d)"), NumVerts, NumTris);

#if WITH_EDITOR
            // 准备缓存数据
            TArray<FVector> CacheVertices;
            TArray<uint32> CacheIndices;
            TMap<FVertexID, uint32> VertexIDToIndex;

            CacheVertices.SetNum(NumVerts);
            CacheIndices.Reserve(NumTris * 3);

            int32 VertIdx = 0;
            for (const FVertexID VertID : MeshDesc->Vertices().GetElementIDs())
            {
                CacheVertices[VertIdx] = FVector(VertexPositions[VertID]);
                VertexIDToIndex.Add(VertID, VertIdx);
                VertIdx++;
          }
#endif

            // 原始方法：直接从 MeshDescription 迭代三角形
      for (const FTriangleID TriID : MeshDesc->Triangles().GetElementIDs())
      {
                TArrayView<const FVertexID> TriVertices = MeshDesc->GetTriangleVertices(TriID);

                // 直接获取顶点位置（原始方法）
          const FVector V0 = FVector(VertexPositions[TriVertices[0]]);
          const FVector V1 = FVector(VertexPositions[TriVertices[1]]);
          const FVector V2 = FVector(VertexPositions[TriVertices[2]]);

#if WITH_EDITOR
                // 收集用于缓存的索引
                CacheIndices.Add(VertexIDToIndex[TriVertices[0]]);
                CacheIndices.Add(VertexIDToIndex[TriVertices[1]]);
                CacheIndices.Add(VertexIDToIndex[TriVertices[2]]);
#endif

                // 原始体素化逻辑
                VoxelizeTriangle(V0, V1, V2, OutLayout, OutSubCellStates);
            }

#if WITH_EDITOR
            // 缓存三角形数据以供运行时使用
            if (!OutLayout.HasCachedTriangleData())
            {
                OutLayout.CachedVertices = MoveTemp(CacheVertices);
                OutLayout.CachedIndices = MoveTemp(CacheIndices);
                UE_LOG(LogTemp, Log, TEXT("VoxelizeWithTriangles: Cached triangle data for runtime use"));
            }
#endif

            UE_LOG(LogTemp, Log, TEXT("VoxelizeWithTriangles: Valid cells = %d"), OutLayout.GetValidCellCount());
            return;
        }
    }

    // 方法 2：回退到包围盒填充
    UE_LOG(LogTemp, Warning, TEXT("VoxelizeWithTriangles: No triangle data available. Falling back to bounding box fill."));

    const int32 TotalCells = OutLayout.GetTotalCellCount();
    for (int32 CellId = 0; CellId < TotalCells; ++CellId)
    {
        OutLayout.SetCellExists(CellId, true);
        OutLayout.RegisterValidCell(CellId);
    }
}

void FGridCellBuilder::VoxelizeTriangle(
    const FVector& V0,
    const FVector& V1,
    const FVector& V2,
    FGridCellLayout& OutLayout,
    TMap<int32, FSubCell>* OutSubCellStates)
{
          // 计算三角形AABB
          FVector TriMin, TriMax;
          TriMin.X = FMath::Min3(V0.X, V1.X, V2.X);
          TriMin.Y = FMath::Min3(V0.Y, V1.Y, V2.Y);
          TriMin.Z = FMath::Min3(V0.Z, V1.Z, V2.Z);
          TriMax.X = FMath::Max3(V0.X, V1.X, V2.X);
          TriMax.Y = FMath::Max3(V0.Y, V1.Y, V2.Y);
          TriMax.Z = FMath::Max3(V0.Z, V1.Z, V2.Z);

          // 计算与三角形AABB重叠的单元格范围
          const int32 MinCellX = FMath::Clamp(
              FMath::FloorToInt((TriMin.X - OutLayout.GridOrigin.X) / OutLayout.CellSize.X),
              0, OutLayout.GridSize.X - 1);
          const int32 MinCellY = FMath::Clamp(
              FMath::FloorToInt((TriMin.Y - OutLayout.GridOrigin.Y) / OutLayout.CellSize.Y),
              0, OutLayout.GridSize.Y - 1);
          const int32 MinCellZ = FMath::Clamp(
              FMath::FloorToInt((TriMin.Z - OutLayout.GridOrigin.Z) / OutLayout.CellSize.Z),
              0, OutLayout.GridSize.Z - 1);

          const int32 MaxCellX = FMath::Clamp(
              FMath::FloorToInt((TriMax.X - OutLayout.GridOrigin.X) / OutLayout.CellSize.X),
              0, OutLayout.GridSize.X - 1);
          const int32 MaxCellY = FMath::Clamp(
              FMath::FloorToInt((TriMax.Y - OutLayout.GridOrigin.Y) / OutLayout.CellSize.Y),
              0, OutLayout.GridSize.Y - 1);
          const int32 MaxCellZ = FMath::Clamp(
              FMath::FloorToInt((TriMax.Z - OutLayout.GridOrigin.Z) / OutLayout.CellSize.Z),
              0, OutLayout.GridSize.Z - 1);

          // 将范围内的所有单元格标记为有效
          for (int32 Z = MinCellZ; Z <= MaxCellZ; Z++)
          {
              for (int32 Y = MinCellY; Y <= MaxCellY; Y++)
              {
                  for (int32 X = MinCellX; X <= MaxCellX; X++)
                  {
					  const int32 CellId = OutLayout.CoordToId(X, Y, Z);
					   
					  FVector CellMin(
						  OutLayout.GridOrigin.X + X * OutLayout.CellSize.X,
						  OutLayout.GridOrigin.Y + Y * OutLayout.CellSize.Y,
						  OutLayout.GridOrigin.Z + Z * OutLayout.CellSize.Z
					  );
					  FVector CellMax = CellMin + OutLayout.CellSize;

					  if (!OutLayout.GetCellExists(CellId))
					  {
						  if (TriangleIntersectsAABB(V0, V1, V2, CellMin, CellMax))
						  {
							  OutLayout.SetCellExists(CellId, true);
							  OutLayout.RegisterValidCell(CellId);

							  if (OutSubCellStates)
							  { 
								  FSubCell& SubCellState = OutSubCellStates->FindOrAdd(CellId);
								  SubCellState.Bits = 0x00;
								  MarkIntersectingSubCellsAlive(V0, V1, V2, CellMin, OutLayout.CellSize, SubCellState);
							  }
						  }
					  }
                // Cell이 이미 존재하는 경우
					  else if (OutSubCellStates)
					  {
						  FSubCell* SubCellState = OutSubCellStates->Find(CellId);
						  // 아직 모든 subcell이 alive가 아니면, 한 번 더 체크
						  if (SubCellState && SubCellState->Bits != 0xFF)
						  {
							  if (TriangleIntersectsAABB(V0, V1, V2, CellMin, CellMax))
							  {
								  MarkIntersectingSubCellsAlive(V0, V1, V2, CellMin, OutLayout.CellSize, *SubCellState);
							  }
						  }
					  }
            }
                  }
              }
}

void FGridCellBuilder::VoxelizeFromArrays(
    const TArray<FVector>& Vertices,
    const TArray<uint32>& Indices,
    FGridCellLayout& OutLayout,
    TMap<int32, FSubCell>* OutSubCellStates)
{
    const uint32 NumVertices = Vertices.Num();
    const uint32 NumTriangles = Indices.Num() / 3;

    for (uint32 TriIdx = 0; TriIdx < NumTriangles; ++TriIdx)
    {
        const uint32 I0 = Indices[TriIdx * 3 + 0];
        const uint32 I1 = Indices[TriIdx * 3 + 1];
        const uint32 I2 = Indices[TriIdx * 3 + 2];

        if (I0 >= NumVertices || I1 >= NumVertices || I2 >= NumVertices)
        {
            continue;
          }

        VoxelizeTriangle(Vertices[I0], Vertices[I1], Vertices[I2], OutLayout, OutSubCellStates);
      }

    UE_LOG(LogTemp, Log, TEXT("VoxelizeFromArrays: Valid cells = %d"), OutLayout.GetValidCellCount());
}

bool FGridCellBuilder::TriangleIntersectsAABB(const FVector& V0, const FVector& V1, const FVector& V2, const FVector& BoxMin, const FVector& BoxMax)
{
	// 假设盒子位于(0,0,0)以简化数学计算
	 
	// 计算盒子的中心和半尺寸
	const FVector BoxCenter = (BoxMin + BoxMax) * 0.5f;

	const FVector BoxEpsilon = (BoxMax - BoxMin) * 0.5f * FVector(0.01f);

	const FVector BoxHalfSize = (BoxMax - BoxMin) * 0.5f + BoxEpsilon;

	// 将三角形相对于盒子中心移动
	const FVector T0 = V0 - BoxCenter;
	const FVector T1 = V1 - BoxCenter;
	const FVector T2 = V2 - BoxCenter;

	// 三角形边缘向量
	const FVector E0 = T1 - T0;
	const FVector E1 = T2 - T1;
	const FVector E2 = T0 - T2;

 	// 1. 3个盒子轴 (X, Y, Z)
 
	// X 轴
	{
		const float Min = FMath::Min3(T0.X, T1.X, T2.X);
		const float Max = FMath::Max3(T0.X, T1.X, T2.X);
		if (Min > BoxHalfSize.X || Max < -BoxHalfSize.X)
		{
			return false;
		}
	}

	// Y 轴
	{
		const float Min = FMath::Min3(T0.Y, T1.Y, T2.Y);
		const float Max = FMath::Max3(T0.Y, T1.Y, T2.Y);
		if (Min > BoxHalfSize.Y || Max < -BoxHalfSize.Y)
		{
			return false;
		}
	}

	// Z 轴
	{
		const float Min = FMath::Min3(T0.Z, T1.Z, T2.Z);
		const float Max = FMath::Max3(T0.Z, T1.Z, T2.Z);
		if (Min > BoxHalfSize.Z || Max < -BoxHalfSize.Z)
		{
			return false;
		}
	}

	// 2. 三角形法线轴
	{
		const FVector Normal = FVector::CrossProduct(E0, E1);
		const float D = FVector::DotProduct(Normal, T0);
		const float R = BoxHalfSize.X * FMath::Abs(Normal.X) +
			BoxHalfSize.Y * FMath::Abs(Normal.Y) +
			BoxHalfSize.Z * FMath::Abs(Normal.Z);
		if (FMath::Abs(D) > R)
		{
			return false;
		}
	}

	// 3. 9个交叉轴 (Cross(盒子轴, 三角形边缘))
	
	// 辅助 lambda：分离轴测试
	auto TestAxis = [&](const FVector& Axis) -> bool
		{
			// 如果轴接近于零，则跳过
			if (Axis.SizeSquared() < KINDA_SMALL_NUMBER)
			{
				return true; // 未分离（测试通过）
			}

			// 将三角形顶点投影到轴上
			const float P0 = FVector::DotProduct(Axis, T0);
			const float P1 = FVector::DotProduct(Axis, T1);
			const float P2 = FVector::DotProduct(Axis, T2);

			const float TriMin = FMath::Min3(P0, P1, P2);
			const float TriMax = FMath::Max3(P0, P1, P2);

			// 将盒子投影到轴上（半范围投影的总和）
			const float BoxR = BoxHalfSize.X * FMath::Abs(Axis.X) +
				BoxHalfSize.Y * FMath::Abs(Axis.Y) +
				BoxHalfSize.Z * FMath::Abs(Axis.Z);

			// 分离轴测试
			if (TriMin > BoxR || TriMax < -BoxR)
			{
				return false; // 已分离！
			}
			return true; // 未分离
		};

	// Cross(X 轴, 边缘)
	if (!TestAxis(FVector(0, -E0.Z, E0.Y))) return false; 
	if (!TestAxis(FVector(0, -E1.Z, E1.Y))) return false; 
	if (!TestAxis(FVector(0, -E2.Z, E2.Y))) return false; 

	// Cross(Y 轴, 边缘)
	if (!TestAxis(FVector(E0.Z, 0, -E0.X))) return false; 
	if (!TestAxis(FVector(E1.Z, 0, -E1.X))) return false; 
	if (!TestAxis(FVector(E2.Z, 0, -E2.X))) return false; 

	// Cross(Z 轴, 边缘)
	if (!TestAxis(FVector(-E0.Y, E0.X, 0))) return false; 
	if (!TestAxis(FVector(-E1.Y, E1.X, 0))) return false; 
	if (!TestAxis(FVector(-E2.Y, E2.X, 0))) return false; 

	// 所有测试通过 == 相交
	return true;
}

void FGridCellBuilder::FillInsideVoxels(FGridCellLayout& OutLayout)
{
	// 访问检查（TBitArray 更节省内存，但为方便起见使用 TSet）
	TSet<int32> VisitedOutside;
	TQueue<int32> Queue;

	const FIntVector GridSize = OutLayout.GridSize;
	 
	// 1. 初始化：将网格的6个边界平面入队（始终是外部空气）
	for (int32 Z = 0; Z < GridSize.Z; ++Z)
	{
		for (int32 Y = 0; Y < GridSize.Y; ++Y)
		{
			for (int32 X = 0; X < GridSize.X; ++X)
			{
				// 检查是否在边界上
				if (X == 0 || X == GridSize.X - 1 ||
					Y == 0 || Y == GridSize.Y - 1 ||
					Z == 0 || Z == GridSize.Z - 1)
				{
					int32 CellId = OutLayout.CoordToId(X, Y, Z);

					// 没有外壳（网格）的边界 -> 绝对是空气
					if (!OutLayout.GetCellExists(CellId))
					{
						Queue.Enqueue(CellId);
						VisitedOutside.Add(CellId);
					}
				}
			}
		}
	} 
	  
	// 用于6向遍历
	static const FIntVector Directions[6] = {
		{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
	};

	// 2. BFS 遍历（传播外部空气）
	int32 CurrentId;
	while (Queue.Dequeue(CurrentId))
	{
		FIntVector CurrentCoord = OutLayout.IdToCoord(CurrentId);

		for (const FIntVector& Dir : Directions)
		{
			FIntVector NextCoord = CurrentCoord + Dir;

			// 如果超出网格则跳过
			if (!OutLayout.IsValidCoord(NextCoord)) continue;

			int32 NextId = OutLayout.CoordToId(NextCoord);

			// 如果已访问（空气）或外壳（墙），则无法继续
			if (VisitedOutside.Contains(NextId) || OutLayout.GetCellExists(NextId))
			{
				continue;
			}

			// 到达空白空间 -> 标记为已访问并入队
			VisitedOutside.Add(NextId);
			Queue.Enqueue(NextId);
		}
	}

	// 3. 反转：空气无法到达的区域是内部
	const int32 TotalCells = OutLayout.GetTotalCellCount(); 

	for (int32 i = 0; i < TotalCells; ++i)
	{
		// 如果已经是外壳则跳过
		if (OutLayout.GetCellExists(i)) continue;

		// 外部空气无法到达 -> 内部
		if (!VisitedOutside.Contains(i))
		{
			OutLayout.SetCellExists(i, true); // 填充
			OutLayout.RegisterValidCell(i); 
		}
	} 

}
bool FGridCellBuilder::IsPointInsideConvex(
	const FKConvexElem& ConvexElem,
	const FVector& Point)
{
	const TArray<FVector>& Vertices = ConvexElem.VertexData;

	// 如果没有 VertexData，则回退到 ElemBox（凸包的包围盒）
	if (Vertices.Num() < 4)
	{
		const FBox& ElemBox = ConvexElem.ElemBox;
		if (ElemBox.IsValid)
		{
			return ElemBox.IsInside(Point);
		}
		return false;
	}

	// 计算包围盒（快速拒绝）
	FBox ConvexBounds(ForceInit);
	FVector Centroid = FVector::ZeroVector;
	for (const FVector& V : Vertices)
	{
		ConvexBounds += V;
		Centroid += V;
	}
	Centroid /= Vertices.Num();

	// 如果在包围盒外，则快速拒绝
	if (!ConvexBounds.IsInside(Point))
	{
		return false;
	}

	const TArray<int32>& IndexData = ConvexElem.IndexData;

	// 如果没有 IndexData，则回退到包围盒检查
	if (IndexData.Num() < 3)
	{
		return true;
	}

	// 使用三角面确定内部
	// 使用质心设置法线方向（质心始终在内部）
	for (int32 i = 0; i + 2 < IndexData.Num(); i += 3)
	{
		if (IndexData[i] >= Vertices.Num() ||
		    IndexData[i+1] >= Vertices.Num() ||
		    IndexData[i+2] >= Vertices.Num())
		{
			continue;
		}

		const FVector& V0 = Vertices[IndexData[i]];
		const FVector& V1 = Vertices[IndexData[i + 1]];
		const FVector& V2 = Vertices[IndexData[i + 2]];

		// 计算面法线
		FVector Normal = FVector::CrossProduct(V1 - V0, V2 - V0).GetSafeNormal();

		// 质心必须在另一侧（法线应朝外）
		// 如果质心位于法线侧，则翻转法线
		const float CentroidDist = FVector::DotProduct(Centroid - V0, Normal);
		if (CentroidDist > 0)
		{
			Normal = -Normal;  // 翻转法线
		}

		// 如果点在任何一个面的外面，它就在凸包的外面
		const float Distance = FVector::DotProduct(Point - V0, Normal);
		if (Distance > KINDA_SMALL_NUMBER)
		{
			return false;
		}
	}

	return true;
}

bool FGridCellBuilder::IsPointInsideBox(
	const FKBoxElem& BoxElem,
	const FVector& Point)
{
	// 计算相对于中心的位置
	FVector LocalPoint = Point - BoxElem.Center;

	// 如果存在旋转，则应用旋转
	if (!BoxElem.Rotation.IsNearlyZero())
	{
		LocalPoint = BoxElem.Rotation.UnrotateVector(LocalPoint);
	}

	// 半范围（X, Y, Z 是完整尺寸，所以减半）
	const FVector HalfExtent(BoxElem.X * 0.5f, BoxElem.Y * 0.5f, BoxElem.Z * 0.5f);

	// AABB 包含检查
	return FMath::Abs(LocalPoint.X) <= HalfExtent.X &&
	       FMath::Abs(LocalPoint.Y) <= HalfExtent.Y &&
	       FMath::Abs(LocalPoint.Z) <= HalfExtent.Z;
}

bool FGridCellBuilder::IsPointInsideSphere(
	const FKSphereElem& SphereElem,
	const FVector& Point)
{
	// 从球心到点的距离
	const FVector Center = SphereElem.Center;
	const float RadiusSq = SphereElem.Radius * SphereElem.Radius;

	return FVector::DistSquared(Point, Center) <= RadiusSq;
}

bool FGridCellBuilder::IsPointInsideCapsule(
	const FKSphylElem& CapsuleElem,
	const FVector& Point)
{
	// 变换到胶囊体局部空间
	const FTransform CapsuleTransform = CapsuleElem.GetTransform();
	const FVector LocalPoint = CapsuleTransform.InverseTransformPosition(Point);

	const float Radius = CapsuleElem.Radius;
	const float HalfLength = CapsuleElem.Length * 0.5f;

	// 假设胶囊体沿 Z 轴对齐
	// 胶囊体 = 圆柱体 + 两个半球

	// 检查 Z 是否在圆柱体或半球区域
	if (FMath::Abs(LocalPoint.Z) <= HalfLength)
	{
		// 圆柱体部分：在 XY 平面中进行距离检查
		const float DistXYSq = LocalPoint.X * LocalPoint.X + LocalPoint.Y * LocalPoint.Y;
		return DistXYSq <= Radius * Radius;
	}
	else
	{
		// 半球部分：从最近的半球中心进行距离检查
		const FVector HemiCenter(0, 0, LocalPoint.Z > 0 ? HalfLength : -HalfLength);
		return FVector::DistSquared(LocalPoint, HemiCenter) <= Radius * Radius;
	}
}

void FGridCellBuilder::CalculateNeighbors(FGridCellLayout& OutLayout)
{
	// 6 个方向 (+/-X, +/-Y, +/-Z)
	static const FIntVector Directions[6] = {
		{1, 0, 0}, {-1, 0, 0},
		{0, 1, 0}, {0, -1, 0},
		{0, 0, 1}, {0, 0, -1}
	};

	// 仅迭代有效单元格（稀疏数组）
	for (int32 CellId : OutLayout.GetValidCellIds())
	{
		const FIntVector Coord = OutLayout.IdToCoord(CellId);
		FIntArray* Neighbors = OutLayout.GetCellNeighborsMutable(CellId);
		if (!Neighbors)
		{
			continue;
		}

		for (const FIntVector& Dir : Directions)
		{
			const FIntVector NeighborCoord = Coord + Dir;

			// 范围检查
			if (!OutLayout.IsValidCoord(NeighborCoord))
			{
				continue;
			}

			const int32 NeighborId = OutLayout.CoordToId(NeighborCoord);

			if (OutLayout.GetCellExists(NeighborId))
			{
				Neighbors->Add(NeighborId);
			}
		}
	}
}

void FGridCellBuilder::DetermineAnchors(
	FGridCellLayout& OutLayout,
	float HeightThreshold)
{
	const float FloorZ = OutLayout.GridOrigin.Z;

	// 仅迭代有效单元格（稀疏数组）
	for (int32 CellId : OutLayout.GetValidCellIds())
	{
		// Z=0 层或接近地面的单元格是锚点
		const FIntVector Coord = OutLayout.IdToCoord(CellId);
		const float CellMinZ = OutLayout.GridOrigin.Z + Coord.Z * OutLayout.CellSize.Z;

		if (CellMinZ - FloorZ <= HeightThreshold)
		{
			OutLayout.SetCellIsAnchor(CellId, true);
		}
	}
}
