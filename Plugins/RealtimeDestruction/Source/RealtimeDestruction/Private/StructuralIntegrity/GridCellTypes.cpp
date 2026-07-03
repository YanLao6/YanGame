// Copyright (c) 2026 LazyDevelopers <lazydeveloper24@gmail.com>. All rights reserved.
// This plugin is distributed under the Fab Standard License.
//
// This product was independently developed by us while participating in the Epic Project, a developer-support
// program of the KRAFTON JUNGLE GameTech Lab. All rights, title, and interest in and to the product are exclusively
// vested in us. Krafton, Inc. was not involved in its development and distribution and disclaims all representations
// and warranties, express or implied, and assumes no responsibility or liability for any consequences arising from
// the use of this product.

#include "StructuralIntegrity/GridCellTypes.h"
#include "Components/RealtimeDestructibleMeshComponent.h"

//=============================================================================
// FDestructionShape
//=============================================================================

bool FCellDestructionShape::ContainsPoint(const FVector& Point) const
{
	switch (Type)
	{
	case ECellDestructionShapeType::Sphere:
		return FVector::DistSquared(Point, Center) <= Radius * Radius;

	case ECellDestructionShapeType::Box:
		{
			// 带旋转
			if (!Rotation.IsNearlyZero())
			{
				// 将点转换到盒子的局部空间
				const FVector LocalPoint = Rotation.UnrotateVector(Point - Center);

				return FMath::Abs(LocalPoint.X) <= BoxExtent.X &&
				       FMath::Abs(LocalPoint.Y) <= BoxExtent.Y &&
				       FMath::Abs(LocalPoint.Z) <= BoxExtent.Z;
			}
			else
			{
				// 无旋转：简单的 AABB 检查
				return FMath::Abs(Point.X - Center.X) <= BoxExtent.X &&
				       FMath::Abs(Point.Y - Center.Y) <= BoxExtent.Y &&
				       FMath::Abs(Point.Z - Center.Z) <= BoxExtent.Z;
			}
		}

	case ECellDestructionShapeType::Cylinder:
		{
			// 圆柱体局部空间中的 XY 距离 + Z 范围检查
			const FVector LocalPoint = Rotation.IsNearlyZero()
				? (Point - Center)
				: Rotation.UnrotateVector(Point - Center);

			const float DistXYSq = FMath::Square(LocalPoint.X) + FMath::Square(LocalPoint.Y);
			return DistXYSq <= Radius * Radius &&
			       FMath::Abs(LocalPoint.Z) <= BoxExtent.Z;
		}

	case ECellDestructionShapeType::Line:
		{
			// 到线段的最短距离
			const FVector LineDir = EndPoint - Center;
			const float LineLength = LineDir.Size();
			if (LineLength < KINDA_SMALL_NUMBER)
			{
				return false;
			}

			const FVector LineDirNorm = LineDir / LineLength;
			const FVector ToPoint = Point - Center;
			const float Projection = FVector::DotProduct(ToPoint, LineDirNorm);

			// 检查线段边界
			if (Projection < 0.0f || Projection > LineLength)
			{
				return false;
			}

			// 距离检查
			const FVector ClosestPoint = Center + LineDirNorm * Projection;
			return FVector::Dist(Point, ClosestPoint) <= LineThickness;
		}
	}

	return false;
}


FCellDestructionShape FCellDestructionShape::CreateFromRequest(const FRealtimeDestructionRequest& Request)
{
	FCellDestructionShape Shape;
	Shape.Center = Request.ImpactPoint;
	Shape.Radius = Request.ShapeParams.Radius;

	switch (Request.ToolShape)
	{
	case EDestructionToolShape::Sphere:
		Shape.Type = ECellDestructionShapeType::Sphere;
		break;

	case EDestructionToolShape::Cylinder:
		Shape.Type = ECellDestructionShapeType::Line;
		Shape.EndPoint = Request.ImpactPoint + Request.ToolForwardVector * Request.ShapeParams.Height;
		Shape.LineThickness = Request.ShapeParams.Radius;
		break;

	default:
		Shape.Type = ECellDestructionShapeType::Sphere;
		break;
	}

	return Shape;
}

//=============================================================================
// FQuantizedDestructionInput
//=============================================================================

FQuantizedDestructionInput FQuantizedDestructionInput::FromDestructionShape(const FCellDestructionShape& Shape)
{
	FQuantizedDestructionInput Result;
	Result.Type = Shape.Type;

	// 将 cm 转换为 mm（量化）
	Result.CenterMM = FIntVector(
		FMath::RoundToInt(Shape.Center.X * 10.0f),
		FMath::RoundToInt(Shape.Center.Y * 10.0f),
		FMath::RoundToInt(Shape.Center.Z * 10.0f)
	);

	Result.RadiusMM = FMath::RoundToInt(Shape.Radius * 10.0f);

	Result.BoxExtentMM = FIntVector(
		FMath::RoundToInt(Shape.BoxExtent.X * 10.0f),
		FMath::RoundToInt(Shape.BoxExtent.Y * 10.0f),
		FMath::RoundToInt(Shape.BoxExtent.Z * 10.0f)
	);

	// 将角度转换为 0.01 度单位
	Result.RotationCentidegrees = FIntVector(
		FMath::RoundToInt(Shape.Rotation.Pitch * 100.0f),
		FMath::RoundToInt(Shape.Rotation.Yaw * 100.0f),
		FMath::RoundToInt(Shape.Rotation.Roll * 100.0f)
	);

	Result.EndPointMM = FIntVector(
		FMath::RoundToInt(Shape.EndPoint.X * 10.0f),
		FMath::RoundToInt(Shape.EndPoint.Y * 10.0f),
		FMath::RoundToInt(Shape.EndPoint.Z * 10.0f)
	);

	Result.LineThicknessMM = FMath::RoundToInt(Shape.LineThickness * 10.0f);

	return Result;
}

FCellDestructionShape FQuantizedDestructionInput::ToDestructionShape() const
{
	FCellDestructionShape Result;
	Result.Type = Type;

	// 将 mm 转换为 cm
	Result.Center = FVector(
		CenterMM.X * 0.1f,
		CenterMM.Y * 0.1f,
		CenterMM.Z * 0.1f
	);

	Result.Radius = RadiusMM * 0.1f;

	Result.BoxExtent = FVector(
		BoxExtentMM.X * 0.1f,
		BoxExtentMM.Y * 0.1f,
		BoxExtentMM.Z * 0.1f
	);

	Result.Rotation = FRotator(
		RotationCentidegrees.X * 0.01f,
		RotationCentidegrees.Y * 0.01f,
		RotationCentidegrees.Z * 0.01f
	);

	Result.EndPoint = FVector(
		EndPointMM.X * 0.1f,
		EndPointMM.Y * 0.1f,
		EndPointMM.Z * 0.1f
	);

	Result.LineThickness = LineThicknessMM * 0.1f;

	return Result;
}

bool FQuantizedDestructionInput::ContainsPoint(const FVector& Point) const
{
	// 使用量化值进行评估
	const FVector Center = FVector(CenterMM.X, CenterMM.Y, CenterMM.Z) * 0.1f;
	const float RadiusCm = RadiusMM * 0.1f;
	const FVector BoxExtentCm = FVector(BoxExtentMM.X, BoxExtentMM.Y, BoxExtentMM.Z) * 0.1f;

	switch (Type)
	{
	case ECellDestructionShapeType::Sphere:
		return FVector::DistSquared(Point, Center) <= RadiusCm * RadiusCm;

	case ECellDestructionShapeType::Box:
		{
			if (RotationCentidegrees != FIntVector::ZeroValue)
			{
				const FRotator Rot(
					RotationCentidegrees.X * 0.01f,
					RotationCentidegrees.Y * 0.01f,
					RotationCentidegrees.Z * 0.01f
				);
				const FVector LocalPoint = Rot.UnrotateVector(Point - Center);

				return FMath::Abs(LocalPoint.X) <= BoxExtentCm.X &&
				       FMath::Abs(LocalPoint.Y) <= BoxExtentCm.Y &&
				       FMath::Abs(LocalPoint.Z) <= BoxExtentCm.Z;
			}
			else
			{
				return FMath::Abs(Point.X - Center.X) <= BoxExtentCm.X &&
				       FMath::Abs(Point.Y - Center.Y) <= BoxExtentCm.Y &&
				       FMath::Abs(Point.Z - Center.Z) <= BoxExtentCm.Z;
			}
		}

	case ECellDestructionShapeType::Cylinder:
		{
			const FRotator Rot(
				RotationCentidegrees.X * 0.01f,
				RotationCentidegrees.Y * 0.01f,
				RotationCentidegrees.Z * 0.01f
			);
			const FVector LocalPoint = Rot.IsNearlyZero()
				? (Point - Center)
				: Rot.UnrotateVector(Point - Center);

			const float DistXYSq = FMath::Square(LocalPoint.X) + FMath::Square(LocalPoint.Y);

			return DistXYSq <= RadiusCm * RadiusCm &&
			       FMath::Abs(LocalPoint.Z) <= BoxExtentCm.Z;
		}

	case ECellDestructionShapeType::Line:
		{
			const FVector EndPt = FVector(EndPointMM.X, EndPointMM.Y, EndPointMM.Z) * 0.1f;
			const float ThicknessCm = LineThicknessMM * 0.1f;

			const FVector LineDir = EndPt - Center;
			const float LineLength = LineDir.Size();
			if (LineLength < KINDA_SMALL_NUMBER)
			{
				return false;
			}

			const FVector LineDirNorm = LineDir / LineLength;
			const FVector ToPoint = Point - Center;
			const float Projection = FVector::DotProduct(ToPoint, LineDirNorm);

			if (Projection < 0.0f || Projection > LineLength)
			{
				return false;
			}

			const FVector ClosestPoint = Center + LineDirNorm * Projection;
			return FVector::Dist(Point, ClosestPoint) <= ThicknessCm;
		}
	}

	return false;
}

bool FQuantizedDestructionInput::IntersectsOBB(const FCellOBB& OBB) const
{
	// 将量化值转换为 cm
	const FVector Center = FVector(CenterMM.X, CenterMM.Y, CenterMM.Z) * 0.1f;
	const float RadiusCm = RadiusMM * 0.1f;
	const FVector BoxExtentCm = FVector(BoxExtentMM.X, BoxExtentMM.Y, BoxExtentMM.Z) * 0.1f;

	switch (Type)
	{
	case ECellDestructionShapeType::Sphere:
		{
			// 球体-OBB 交集：检查 OBB 上最近的点是否在球体内部
			const FVector ClosestPoint = OBB.GetClosestPoint(Center);
			return FVector::DistSquared(ClosestPoint, Center) <= RadiusCm * RadiusCm;
		}

	case ECellDestructionShapeType::Box:
		{
			// OBB 与 OBB 交集使用 SAT（分离轴定理）
			// 15 轴测试：每个盒子 3 个轴 + 9 个边缘交叉轴

			// 计算形状旋转和轴
			FQuat ShapeQuat = FQuat::Identity;
			if (RotationCentidegrees != FIntVector::ZeroValue)
			{
				const FRotator ShapeRot(
					RotationCentidegrees.X * 0.01f,
					RotationCentidegrees.Y * 0.01f,
					RotationCentidegrees.Z * 0.01f
				);
				ShapeQuat = ShapeRot.Quaternion();
			}

			const FVector ShapeAxes[3] = {
				ShapeQuat.RotateVector(FVector::ForwardVector),
				ShapeQuat.RotateVector(FVector::RightVector),
				ShapeQuat.RotateVector(FVector::UpVector)
			};

			const FVector OBBAxes[3] = { OBB.AxisX, OBB.AxisY, OBB.AxisZ };

			// 盒子中心之间的向量
			const FVector D = OBB.Center - Center;

			// 在 15 个轴上进行分离轴测试
			auto TestAxis = [&](const FVector& Axis) -> bool
			{
				if (Axis.SizeSquared() < KINDA_SMALL_NUMBER)
				{
					return true; // 如果轴太小，则视为不可分离
				}

				const FVector NormAxis = Axis.GetSafeNormal();

				// 形状盒子的投影半径
				float ShapeProjection = 0.0f;
				ShapeProjection += FMath::Abs(FVector::DotProduct(ShapeAxes[0], NormAxis)) * BoxExtentCm.X;
				ShapeProjection += FMath::Abs(FVector::DotProduct(ShapeAxes[1], NormAxis)) * BoxExtentCm.Y;
				ShapeProjection += FMath::Abs(FVector::DotProduct(ShapeAxes[2], NormAxis)) * BoxExtentCm.Z;

				// OBB 的投影半径
				float OBBProjection = 0.0f;
				OBBProjection += FMath::Abs(FVector::DotProduct(OBBAxes[0], NormAxis)) * OBB.HalfExtents.X;
				OBBProjection += FMath::Abs(FVector::DotProduct(OBBAxes[1], NormAxis)) * OBB.HalfExtents.Y;
				OBBProjection += FMath::Abs(FVector::DotProduct(OBBAxes[2], NormAxis)) * OBB.HalfExtents.Z;

				// 中心距离的投影
				const float CenterDistance = FMath::Abs(FVector::DotProduct(D, NormAxis));

				// 分离轴测试：如果中心距离 > 半径之和，则分离
				return CenterDistance <= ShapeProjection + OBBProjection;
			};

			// 测试形状盒子轴
			for (int32 i = 0; i < 3; ++i)
			{
				if (!TestAxis(ShapeAxes[i]))
				{
					return false;
				}
			}

			// 测试 OBB 轴
			for (int32 i = 0; i < 3; ++i)
			{
				if (!TestAxis(OBBAxes[i]))
				{
					return false;
				}
			}

			// 测试 9 个边缘交叉轴 (ShapeAxis x OBBAxis)
			for (int32 i = 0; i < 3; ++i)
			{
				for (int32 j = 0; j < 3; ++j)
				{
					const FVector CrossAxis = FVector::CrossProduct(ShapeAxes[i], OBBAxes[j]);
					if (!TestAxis(CrossAxis))
					{
						return false;
					}
				}
			}

			// 未找到分离轴 = 相交
			return true;
		}

	case ECellDestructionShapeType::Cylinder:
		{
			// 圆柱体局部空间中的圆柱体-OBB 交集
			FQuat ShapeQuat = FQuat::Identity;
			if (RotationCentidegrees != FIntVector::ZeroValue)
			{
				const FRotator ShapeRot(
					RotationCentidegrees.X * 0.01f,
					RotationCentidegrees.Y * 0.01f,
					RotationCentidegrees.Z * 0.01f
				);
				ShapeQuat = ShapeRot.Quaternion();
			}

			const FQuat InvQuat = ShapeQuat.Inverse();
			const FVector LocalCenter = InvQuat.RotateVector(OBB.Center - Center);
			const FVector LocalAxisX = InvQuat.RotateVector(OBB.AxisX);
			const FVector LocalAxisY = InvQuat.RotateVector(OBB.AxisY);
			const FVector LocalAxisZ = InvQuat.RotateVector(OBB.AxisZ);

			// Z 范围检查：局部 OBB Z 投影与圆柱体 Z 范围
			float OBBMinZ = FLT_MAX, OBBMaxZ = -FLT_MAX;
			for (int32 i = 0; i < 8; ++i)
			{
				const FVector LocalCorner(
					((i & 1) ? OBB.HalfExtents.X : -OBB.HalfExtents.X),
					((i & 2) ? OBB.HalfExtents.Y : -OBB.HalfExtents.Y),
					((i & 4) ? OBB.HalfExtents.Z : -OBB.HalfExtents.Z)
				);
				const FVector WorldCorner = LocalCenter
					+ LocalAxisX * LocalCorner.X
					+ LocalAxisY * LocalCorner.Y
					+ LocalAxisZ * LocalCorner.Z;
				OBBMinZ = FMath::Min(OBBMinZ, WorldCorner.Z);
				OBBMaxZ = FMath::Max(OBBMaxZ, WorldCorner.Z);
			}

			if (OBBMaxZ < -BoxExtentCm.Z || OBBMinZ > BoxExtentCm.Z)
			{
				return false;
			}

			// XY 平面圆与 OBB 在局部空间中的投影
			float MinDistSq = FLT_MAX;
			for (int32 i = 0; i < 8; ++i)
			{
				const FVector LocalCorner(
					((i & 1) ? OBB.HalfExtents.X : -OBB.HalfExtents.X),
					((i & 2) ? OBB.HalfExtents.Y : -OBB.HalfExtents.Y),
					((i & 4) ? OBB.HalfExtents.Z : -OBB.HalfExtents.Z)
				);
				const FVector WorldCorner = LocalCenter
					+ LocalAxisX * LocalCorner.X
					+ LocalAxisY * LocalCorner.Y
					+ LocalAxisZ * LocalCorner.Z;
				const float DistSq = FMath::Square(WorldCorner.X) + FMath::Square(WorldCorner.Y);
				MinDistSq = FMath::Min(MinDistSq, DistSq);
			}

			if (MinDistSq <= RadiusCm * RadiusCm)
			{
				return true;
			}

			const float CenterDistSq = FMath::Square(LocalCenter.X) + FMath::Square(LocalCenter.Y);
			if (CenterDistSq <= RadiusCm * RadiusCm)
			{
				return true;
			}

			// 保守近似：与投影 OBB 半径进行比较
			const float OBBRadiusXY = FMath::Sqrt(
				FMath::Square(OBB.HalfExtents.X * LocalAxisX.X + OBB.HalfExtents.Y * LocalAxisY.X) +
				FMath::Square(OBB.HalfExtents.X * LocalAxisX.Y + OBB.HalfExtents.Y * LocalAxisY.Y)
			) + FMath::Sqrt(
				FMath::Square(OBB.HalfExtents.Z * LocalAxisZ.X) +
				FMath::Square(OBB.HalfExtents.Z * LocalAxisZ.Y)
			);

			return CenterDistSq <= FMath::Square(RadiusCm + OBBRadiusXY);
		}

	case ECellDestructionShapeType::Line:
		{
 
		// 转换为 cm
		const FVector EndPt = FVector(EndPointMM.X, EndPointMM.Y, EndPointMM.Z) * 0.1f;
		const float ThicknessCm = LineThicknessMM * 0.1f;

		// 第一遍过滤
		FCellOBB TestOBB = OBB;
		TestOBB.HalfExtents = OBB.HalfExtents + FVector(ThicknessCm);

		// 将世界坐标转换为局部坐标
		const FVector LocalStart = TestOBB.WorldToLocal(Center);
		const FVector LocalEnd = TestOBB.WorldToLocal(EndPt);
		const FVector LocalDir = LocalEnd - LocalStart; 

		// 在平板测试之前，检查到中心线的距离，如果超出半径则拒绝
		 
		// 为更宽松的边界填充 subcell 大小
		const float SubCellRadius = OBB.HalfExtents.Size();
		const float HitRadius = ThicknessCm + SubCellRadius;

		const float DistToCenterSq = FMath::PointDistToSegmentSquared(FVector::ZeroVector, LocalStart, LocalEnd);
		if (DistToCenterSq > HitRadius * HitRadius)
		{
			return false; // 超出圆形范围
		}
		 
		// 原始逻辑
		float tMin = 0.0f;
		float tMax = 1.0f;

		// 计算每个轴的平板交集
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			float Start, Dir, Extent;
			switch (Axis)
			{
			case 0: Start = LocalStart.X; Dir = LocalDir.X; Extent = TestOBB.HalfExtents.X; break;
			case 1: Start = LocalStart.Y; Dir = LocalDir.Y; Extent = TestOBB.HalfExtents.Y; break;
			case 2: Start = LocalStart.Z; Dir = LocalDir.Z; Extent = TestOBB.HalfExtents.Z; break;
			default: continue;
			}

			if (FMath::Abs(Dir) < KINDA_SMALL_NUMBER)
			{
				// 射线与平板平行
				if (Start < -Extent || Start > Extent)
				{
					return false; // 平行于平板外 = 无交集
				}
			}
			else
			{
				float t1 = (-Extent - Start) / Dir;
				float t2 = (Extent - Start) / Dir;

				if (t1 > t2) Swap(t1, t2);

				tMin = FMath::Max(tMin, t1);
				tMax = FMath::Min(tMax, t2);

				if (tMin > tMax)
				{
					return false; // 无交集区间
				}
			}
		}

		return true; // 通过平板（长度）和距离（厚度）测试
		}
	}

	return false;
}

//=============================================================================
// FGridCellLayout
//=============================================================================

int32 FGridCellLayout::GetAnchorCount() const
{
	int32 Count = 0;
	// 仅迭代有效单元格（稀疏数组）
	for (int32 CellId : SparseIndexToCellId)
	{
		if (GetCellIsAnchor(CellId))
		{
			Count++;
		}
	}
	return Count;
}

int32 FGridCellLayout::WorldPosToId(const FVector& WorldPos, const FTransform& MeshTransform) const
{
	// 将世界坐标转换为局部坐标
	const FVector LocalPos = MeshTransform.InverseTransformPosition(WorldPos);

	// 计算网格坐标
	const int32 X = FMath::FloorToInt((LocalPos.X - GridOrigin.X) / CellSize.X);
	const int32 Y = FMath::FloorToInt((LocalPos.Y - GridOrigin.Y) / CellSize.Y);
	const int32 Z = FMath::FloorToInt((LocalPos.Z - GridOrigin.Z) / CellSize.Z);

	// 范围检查
	if (X < 0 || X >= GridSize.X ||
	    Y < 0 || Y >= GridSize.Y ||
	    Z < 0 || Z >= GridSize.Z)
	{
		return INDEX_NONE;
	}

	return CoordToId(X, Y, Z);
}

FVector FGridCellLayout::IdToWorldCenter(int32 CellId, const FTransform& MeshTransform) const
{
	const FVector LocalCenter = IdToLocalCenter(CellId);
	return MeshTransform.TransformPosition(LocalCenter);
}

FVector FGridCellLayout::IdToLocalCenter(int32 CellId) const
{
	if (!IsValidCellId(CellId))
	{
		return FVector::ZeroVector;
	}

	const FIntVector Coord = IdToCoord(CellId);
	return FVector(
		GridOrigin.X + (Coord.X + 0.5f) * CellSize.X,
		GridOrigin.Y + (Coord.Y + 0.5f) * CellSize.Y,
		GridOrigin.Z + (Coord.Z + 0.5f) * CellSize.Z
	);
}

FVector FGridCellLayout::IdToWorldMin(int32 CellId, const FTransform& MeshTransform) const
{
	const FVector LocalMin = IdToLocalMin(CellId);
	return MeshTransform.TransformPosition(LocalMin);
}

FVector FGridCellLayout::IdToLocalMin(int32 CellId) const
{
	if (!IsValidCellId(CellId))
	{
		return FVector::ZeroVector;
	}

	const FIntVector Coord = IdToCoord(CellId);
	return FVector(
		GridOrigin.X + Coord.X * CellSize.X,
		GridOrigin.Y + Coord.Y * CellSize.Y,
		GridOrigin.Z + Coord.Z * CellSize.Z
	);
}

TArray<FVector> FGridCellLayout::GetCellVertices(int32 CellId) const
{
	TArray<FVector> Vertices;
	Vertices.Reserve(8);

	const FVector Min = IdToLocalMin(CellId);

	// 8 个顶点（位操作选择轴偏移）
	for (int32 i = 0; i < 8; i++)
	{
		Vertices.Add(FVector(
			Min.X + ((i & 1) ? CellSize.X : 0.0f),
			Min.Y + ((i & 2) ? CellSize.Y : 0.0f),
			Min.Z + ((i & 4) ? CellSize.Z : 0.0f)
		));
	}

	return Vertices;
}

void FGridCellLayout::Reset()
{
	GridSize = FIntVector::ZeroValue;
	GridOrigin = FVector::ZeroVector;
	MeshScale = FVector::OneVector;

	// 初始化位域
	CellExistsBits.Empty();
	CellIsAnchorBits.Empty();

	// 初始化稀疏数组
	CellIdToSparseIndex.Empty();
	SparseIndexToCellId.Empty();
	SparseCellTriangles.Empty();
	SparseCellNeighbors.Empty();

	// 注意：此处不要清除 CachedVertices/CachedIndices
	// 它们需要为运行时重建保留
}

bool FGridCellLayout::IsValid() const
{
	if (GridSize.X <= 0 || GridSize.Y <= 0 || GridSize.Z <= 0)
	{
		return false;
	}

	const int32 TotalCells = GetTotalCellCount();
	const int32 RequiredWords = (TotalCells + 31) >> 5;  // ceil(TotalCells / 32)

	// 验证位域大小
	if (CellExistsBits.Num() != RequiredWords || CellIsAnchorBits.Num() != RequiredWords)
	{
		return false;
	}

	// 验证稀疏数组一致性
	const int32 ValidCellCount = SparseIndexToCellId.Num();
	return SparseCellTriangles.Num() == ValidCellCount &&
	       SparseCellNeighbors.Num() == ValidCellCount &&
	       CellIdToSparseIndex.Num() == ValidCellCount;
}

TArray<int32> FGridCellLayout::GetCellsInAABB(const FBox& WorldAABB, const FTransform& MeshTransform) const
{
	TArray<int32> Result;

	if (!IsValid())
	{
		return Result;
	}

	// 将 8 个世界 AABB 角点转换到局部空间以构建局部 AABB
	FBox LocalAABB(ForceInit);

	const FVector WorldCorners[8] = {
		FVector(WorldAABB.Min.X, WorldAABB.Min.Y, WorldAABB.Min.Z),
		FVector(WorldAABB.Max.X, WorldAABB.Min.Y, WorldAABB.Min.Z),
		FVector(WorldAABB.Min.X, WorldAABB.Max.Y, WorldAABB.Min.Z),
		FVector(WorldAABB.Max.X, WorldAABB.Max.Y, WorldAABB.Min.Z),
		FVector(WorldAABB.Min.X, WorldAABB.Min.Y, WorldAABB.Max.Z),
		FVector(WorldAABB.Max.X, WorldAABB.Min.Y, WorldAABB.Max.Z),
		FVector(WorldAABB.Min.X, WorldAABB.Max.Y, WorldAABB.Max.Z),
		FVector(WorldAABB.Max.X, WorldAABB.Max.Y, WorldAABB.Max.Z),
	};

	for (int32 i = 0; i < 8; ++i)
	{
		LocalAABB += MeshTransform.InverseTransformPosition(WorldCorners[i]);
	}

	// 将局部 AABB 转换为网格坐标范围
	const int32 MinX = FMath::Max(0, FMath::FloorToInt((LocalAABB.Min.X - GridOrigin.X) / CellSize.X));
	const int32 MinY = FMath::Max(0, FMath::FloorToInt((LocalAABB.Min.Y - GridOrigin.Y) / CellSize.Y));
	const int32 MinZ = FMath::Max(0, FMath::FloorToInt((LocalAABB.Min.Z - GridOrigin.Z) / CellSize.Z));

	const int32 MaxX = FMath::Min(GridSize.X - 1, FMath::FloorToInt((LocalAABB.Max.X - GridOrigin.X) / CellSize.X));
	const int32 MaxY = FMath::Min(GridSize.Y - 1, FMath::FloorToInt((LocalAABB.Max.Y - GridOrigin.Y) / CellSize.Y));
	const int32 MaxZ = FMath::Min(GridSize.Z - 1, FMath::FloorToInt((LocalAABB.Max.Z - GridOrigin.Z) / CellSize.Z));

	// 收集范围内的所有单元格
	Result.Reserve((MaxX - MinX + 1) * (MaxY - MinY + 1) * (MaxZ - MinZ + 1));

	for (int32 Z = MinZ; Z <= MaxZ; ++Z)
	{
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const int32 CellId = CoordToId(X, Y, Z);
				if (GetCellExists(CellId))
				{
					Result.Add(CellId);
				}
			}
		}
	}

	return Result;
}

//=============================================================================
// FSuperCellState
//=============================================================================

bool FSuperCellState::IsCellOnSupercellBoundary(const FIntVector& CellCoord, const FIntVector& SupercellCoord) const
{
	// SuperCell 内的单元格局部坐标
	const int32 LocalX = CellCoord.X - SupercellCoord.X * SupercellSize.X;
	const int32 LocalY = CellCoord.Y - SupercellCoord.Y * SupercellSize.Y;
	const int32 LocalZ = CellCoord.Z - SupercellCoord.Z * SupercellSize.Z;

	// 边界检查：局部坐标为 0 或 (Size - 1)
	return LocalX == 0 || LocalX == SupercellSize.X - 1 ||
	       LocalY == 0 || LocalY == SupercellSize.Y - 1 ||
	       LocalZ == 0 || LocalZ == SupercellSize.Z - 1;
}

void FSuperCellState::GetCellsInSupercell(int32 SupercellId, const FGridCellLayout& GridCache, TArray<int32>& OutCellIds) const
{
	OutCellIds.Reset();

	if (!IsValidSupercellId(SupercellId))
	{
		return;
	}

	const FIntVector SupercellCoord = SupercellIdToCoord(SupercellId);

	// 计算 SuperCell 单元格坐标范围
	const int32 StartX = SupercellCoord.X * SupercellSize.X;
	const int32 StartY = SupercellCoord.Y * SupercellSize.Y;
	const int32 StartZ = SupercellCoord.Z * SupercellSize.Z;

	const int32 EndX = FMath::Min(StartX + SupercellSize.X, GridCache.GridSize.X);
	const int32 EndY = FMath::Min(StartY + SupercellSize.Y, GridCache.GridSize.Y);
	const int32 EndZ = FMath::Min(StartZ + SupercellSize.Z, GridCache.GridSize.Z);

	OutCellIds.Reserve((EndX - StartX) * (EndY - StartY) * (EndZ - StartZ));

	for (int32 Z = StartZ; Z < EndZ; ++Z)
	{
		for (int32 Y = StartY; Y < EndY; ++Y)
		{
			for (int32 X = StartX; X < EndX; ++X)
			{
				const int32 CellId = GridCache.CoordToId(X, Y, Z);
				if (GridCache.GetCellExists(CellId))
				{
					OutCellIds.Add(CellId);
				}
			}
		}
	}
}

void FSuperCellState::GetBoundaryCellsOfSupercell(int32 SupercellId, const FGridCellLayout& GridCache, TArray<int32>& OutCellIds) const
{
	OutCellIds.Reset();

	if (!IsValidSupercellId(SupercellId))
	{
		return;
	}

	const FIntVector SupercellCoord = SupercellIdToCoord(SupercellId);

	// 计算 SuperCell 单元格坐标范围
	const int32 StartX = SupercellCoord.X * SupercellSize.X;
	const int32 StartY = SupercellCoord.Y * SupercellSize.Y;
	const int32 StartZ = SupercellCoord.Z * SupercellSize.Z;

	const int32 EndX = FMath::Min(StartX + SupercellSize.X, GridCache.GridSize.X);
	const int32 EndY = FMath::Min(StartY + SupercellSize.Y, GridCache.GridSize.Y);
	const int32 EndZ = FMath::Min(StartZ + SupercellSize.Z, GridCache.GridSize.Z);

	// 仅收集边界单元格（6个面）
	TSet<int32> BoundaryCellSet;

	for (int32 Z = StartZ; Z < EndZ; ++Z)
	{
		for (int32 Y = StartY; Y < EndY; ++Y)
		{
			for (int32 X = StartX; X < EndX; ++X)
			{
				// 边界检查
				const bool bOnBoundary = (X == StartX || X == EndX - 1 ||
				                          Y == StartY || Y == EndY - 1 ||
				                          Z == StartZ || Z == EndZ - 1);

				if (bOnBoundary)
				{
					const int32 CellId = GridCache.CoordToId(X, Y, Z);
					if (GridCache.GetCellExists(CellId))
					{
						BoundaryCellSet.Add(CellId);
					}
				}
			}
		}
	}

	OutCellIds = BoundaryCellSet.Array();
}

void FSuperCellState::BuildFromGridLayout(const FGridCellLayout& GridCache)
{
	Reset();

	if (!GridCache.IsValid())
	{
		return;
	}

	// 计算 SuperCellSize: min(GridSize, 8)
	SupercellSize = FIntVector(8, 8, 8);

	// SuperCell 需要沿每个轴完全填充 SupercellSize
	// 示例：GridSize.X = 5, SupercellSize.X = 4 -> SupercellCount.X = 1 (1 个剩余为孤立)
	// 即使我们没有填充所有的 SuperCell，我们仍然会把它们作为 SuperCell，但 intact 将为 false。
	SupercellCount = FIntVector(
		(GridCache.GridSize.X  + SupercellSize.X - 1) / SupercellSize.X,
		(GridCache.GridSize.Y  + SupercellSize.Y - 1) / SupercellSize.Y,
		(GridCache.GridSize.Z  + SupercellSize.Z - 1) / SupercellSize.Z
	);

	//SupercellCount = FIntVector(
	//	(GridCache.GridSize.X) / SupercellSize.X,
	//	(GridCache.GridSize.Y) / SupercellSize.Y,
	//	(GridCache.GridSize.Z) / SupercellSize.Z
	//);


	// 初始化 Cell -> SuperCell 映射
	const int32 TotalCells = GridCache.GetTotalCellCount();
	CellToSupercell.SetNum(TotalCells);

	// 将所有单元格初始化为 INDEX_NONE（孤立）
	for (int32 i = 0; i < TotalCells; ++i)
	{
		CellToSupercell[i] = INDEX_NONE;
	}


	// 初始化 intact 位域（所有 SuperCell 完整）
	InitializeIntactBits();

	InitialValidCellCounts.SetNumZeroed(SupercellCount.X * SupercellCount.Y * SupercellCount.Z);
	DestroyedCellCounts.SetNumZeroed(SupercellCount.X * SupercellCount.Y * SupercellCount.Z);

	const int32 RequiredCellCount = SupercellSize.X * SupercellSize.Y * SupercellSize.Z;
	// 映射属于 SuperCell 的单元格
	for (int32 SCZ = 0; SCZ < SupercellCount.Z; ++SCZ)
	{
		for (int32 SCY = 0; SCY < SupercellCount.Y; ++SCY)
		{
			for (int32 SCX = 0; SCX < SupercellCount.X; ++SCX)
			{

				const int32 SupercellId = SupercellCoordToId(SCX, SCY, SCZ);

				const int32 StartX = SCX * SupercellSize.X;
				const int32 StartY = SCY * SupercellSize.Y;
				const int32 StartZ = SCZ * SupercellSize.Z;

				// 步骤 1：计数有效单元格
				int32 ValidCount = 0;
				for (int32 LZ = 0; LZ < SupercellSize.Z; ++LZ)
				{
					for (int32 LY = 0; LY < SupercellSize.Y; ++LY)
					{
						for (int32 LX = 0; LX < SupercellSize.X; ++LX)
						{
							const int32 GX = StartX + LX;
							const int32 GY = StartY + LY;
							const int32 GZ = StartZ + LZ;

							// 범위 체크 추가 (Korean comment, leave as is)
							if (GX >= GridCache.GridSize.X || GY >= GridCache.GridSize.Y || GZ >= GridCache.GridSize.Z)
							{
								continue;
							}

							const int32 CellId = GridCache.CoordToId(GX, GY, GZ);
							if (GridCache.GetCellExists(CellId))
							{
								ValidCount++;
							}
						}
					}
				}

				// 步骤 2：仅当所有 512 个都存在时才创建 SuperCell
				if (ValidCount == RequiredCellCount)
				{
					for (int32 LZ = 0; LZ < SupercellSize.Z; ++LZ)
					{
						for (int32 LY = 0; LY < SupercellSize.Y; ++LY)
						{
							for (int32 LX = 0; LX < SupercellSize.X; ++LX)
							{
								const int32 CellId = GridCache.CoordToId(StartX + LX, StartY + LY, StartZ + LZ);
								CellToSupercell[CellId] = SupercellId;
								InitialValidCellCounts[SupercellId]++;
							}
						}
					}
				}
				else
				{ 
					// 部分填充的 SuperCell：仅映射有效单元格并处理损坏的单元格。
					for (int32 LZ = 0; LZ < SupercellSize.Z; ++LZ)
					{
						for (int32 LY = 0; LY < SupercellSize.Y; ++LY)
						{
							for (int32 LX = 0; LX < SupercellSize.X; ++LX)
							{
								const int32 GX = StartX + LX;
								const int32 GY = StartY + LY;
								const int32 GZ = StartZ + LZ;
								// 网格范围检查（可能因向上取整而超出）
								if (GX >= GridCache.GridSize.X || GY >= GridCache.GridSize.Y || GZ >= GridCache.GridSize.Z)
								{
									continue;
								}
								const int32 CellId = GridCache.CoordToId(GX, GY, GZ);
								if (GridCache.GetCellExists(CellId))
								{
									CellToSupercell[CellId] = SupercellId;
									InitialValidCellCounts[SupercellId]++;
								}
							}
						}
					}

					MarkSupercellBroken(SupercellId);
				}
			}
		}
	}

	// 构建孤立单元格列表（不属于任何 SuperCell 的有效单元格）
	OrphanCellIds.Reset();
	for (int32 CellId : GridCache.GetValidCellIds())
	{
		if (CellToSupercell[CellId] == INDEX_NONE)
		{
			OrphanCellIds.Add(CellId);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("FSuperCellState::BuildFromGridLayout - GridSize: (%d, %d, %d), SupercellSize: (%d, %d, %d), SupercellCount: (%d, %d, %d), TotalSupercells: %d, OrphanCells: %d"),
		GridCache.GridSize.X, GridCache.GridSize.Y, GridCache.GridSize.Z,
		SupercellSize.X, SupercellSize.Y, SupercellSize.Z,
		SupercellCount.X, SupercellCount.Y, SupercellCount.Z,
		GetTotalSupercellCount(),
		OrphanCellIds.Num());
}

void FSuperCellState::InitializeIntactBits()
{
	const int32 TotalSupercells = GetTotalSupercellCount();
	const int32 RequiredWords = (TotalSupercells + 63) >> 6;  // ceil(TotalSupercells / 64)

	// 将所有位设置为 1（完整）
	IntactBits.SetNum(RequiredWords);
	for (int32 i = 0; i < RequiredWords; ++i)
	{
		IntactBits[i] = ~0ull;  // 所有位都设置
	}
}

void FSuperCellState::Reset()
{
	SupercellSize = FIntVector(4, 4, 4);
	SupercellCount = FIntVector::ZeroValue;
	IntactBits.Empty();
	CellToSupercell.Empty();
	OrphanCellIds.Empty();
}

bool FSuperCellState::IsValid() const
{
	if (SupercellCount.X <= 0 || SupercellCount.Y <= 0 || SupercellCount.Z <= 0)
	{
		return false;
	}

	const int32 TotalSupercells = GetTotalSupercellCount();
	const int32 RequiredWords = (TotalSupercells + 63) >> 6;

	return IntactBits.Num() == RequiredWords && CellToSupercell.Num() > 0;
}

bool FSuperCellState::IsSupercellTrulyIntact(
	int32 SupercellId,
	const FGridCellLayout& GridCache,
	const FCellState& CellState,
	bool bEnableSubcell) const
{
	if (!IsValidSupercellId(SupercellId))
	{
		return false;
	}

	// 如果已标记为损坏，则提前退出
	if (!IsSupercellIntact(SupercellId))
	{
		return false;
	}

	// 检查 SuperCell 中的所有单元格
	const FIntVector SupercellCoord = SupercellIdToCoord(SupercellId);

	const int32 StartX = SupercellCoord.X * SupercellSize.X;
	const int32 StartY = SupercellCoord.Y * SupercellSize.Y;
	const int32 StartZ = SupercellCoord.Z * SupercellSize.Z;

	const int32 EndX = FMath::Min(StartX + SupercellSize.X, GridCache.GridSize.X);
	const int32 EndY = FMath::Min(StartY + SupercellSize.Y, GridCache.GridSize.Y);
	const int32 EndZ = FMath::Min(StartZ + SupercellSize.Z, GridCache.GridSize.Z);

	for (int32 Z = StartZ; Z < EndZ; ++Z)
	{
		for (int32 Y = StartY; Y < EndY; ++Y)
		{
			for (int32 X = StartX; X < EndX; ++X)
			{
				const int32 CellId = GridCache.CoordToId(X, Y, Z);

				// 如果单元格不存在则跳过
				if (!GridCache.GetCellExists(CellId))
				{
					continue;
				}

				// 如果单元格被破坏，则为损坏状态
				if (CellState.DestroyedCells.Contains(CellId))
				{
					return false;
				}

				// 仅在 subcell 模式下检查 subcell 状态
				if (bEnableSubcell)
				{
					const FSubCell* SubCellState = CellState.SubCellStates.Find(CellId);
					if (SubCellState)
					{
						// 0xFF = 所有 subcells 存活 (所有 8 位都设置)
						if (SubCellState->Bits != 0xFF)
						{
							return false;
						}
					}
				}
			}
		}
	}

	return true;
}

void FSuperCellState::UpdateSupercellStates(const TArray<int32>& AffectedCellIds)
{
	for (int32 CellId : AffectedCellIds)
	{
		const int32 SupercellId = GetSupercellForCell(CellId);
		if (SupercellId != INDEX_NONE)
		{
			MarkSupercellBroken(SupercellId);
		}
	}
}

void FSuperCellState::OnCellDestroyed(int32 CellId)
{
	const int32 SupercellId = GetSupercellForCell(CellId);
	if (SupercellId != INDEX_NONE)
	{
		MarkSupercellBroken(SupercellId);
	}
}

void FSuperCellState::OnSubCellDestroyed(int32 CellId, int32 SubCellId)
{
	const int32 SupercellId = GetSupercellForCell(CellId);
	if (SupercellId != INDEX_NONE)
	{
		MarkSupercellBroken(SupercellId);
	}
}

void FSuperCellState::GetBoundaryCellsInDirection(
	int32 SupercellId,
	int32 Direction,
	const FGridCellLayout& GridCache,
	TArray<int32>& OutCellIds) const
{
	OutCellIds.Reset();

	if (!IsValidSupercellId(SupercellId) || Direction < 0 || Direction >= 6)
	{
		return;
	}

	const FIntVector SupercellCoord = SupercellIdToCoord(SupercellId);

	// 计算 SuperCell 单元格坐标范围
	const int32 StartX = SupercellCoord.X * SupercellSize.X;
	const int32 StartY = SupercellCoord.Y * SupercellSize.Y;
	const int32 StartZ = SupercellCoord.Z * SupercellSize.Z;

	const int32 EndX = FMath::Min(StartX + SupercellSize.X, GridCache.GridSize.X);
	const int32 EndY = FMath::Min(StartY + SupercellSize.Y, GridCache.GridSize.Y);
	const int32 EndZ = FMath::Min(StartZ + SupercellSize.Z, GridCache.GridSize.Z);

	// 提取每个方向的边界单元格
	switch (Direction)
	{
	case 0: // -X
		for (int32 Z = StartZ; Z < EndZ; ++Z)
		{
			for (int32 Y = StartY; Y < EndY; ++Y)
			{
				const int32 CellId = GridCache.CoordToId(StartX, Y, Z);
				if (GridCache.GetCellExists(CellId))
				{
					OutCellIds.Add(CellId);
				}
			}
		}
		break;

	case 1: // +X
		for (int32 Z = StartZ; Z < EndZ; ++Z)
		{
			for (int32 Y = StartY; Y < EndY; ++Y)
			{
				const int32 CellId = GridCache.CoordToId(EndX - 1, Y, Z);
				if (GridCache.GetCellExists(CellId))
				{
					OutCellIds.Add(CellId);
				}
			}
		}
		break;

	case 2: // -Y
		for (int32 Z = StartZ; Z < EndZ; ++Z)
		{
			for (int32 X = StartX; X < EndX; ++X)
			{
				const int32 CellId = GridCache.CoordToId(X, StartY, Z);
				if (GridCache.GetCellExists(CellId))
				{
					OutCellIds.Add(CellId);
				}
			}
		}
		break;

	case 3: // +Y
		for (int32 Z = StartZ; Z < EndZ; ++Z)
		{
			for (int32 X = StartX; X < EndX; ++X)
			{
				const int32 CellId = GridCache.CoordToId(X, EndY - 1, Z);
				if (GridCache.GetCellExists(CellId))
				{
					OutCellIds.Add(CellId);
				}
			}
		}
		break;

	case 4: // -Z
		for (int32 Y = StartY; Y < EndY; ++Y)
		{
			for (int32 X = StartX; X < EndX; ++X)
			{
				const int32 CellId = GridCache.CoordToId(X, Y, StartZ);
				if (GridCache.GetCellExists(CellId))
				{
					OutCellIds.Add(CellId);
				}
			}
		}
		break;

	case 5: // +Z
		for (int32 Y = StartY; Y < EndY; ++Y)
		{
			for (int32 X = StartX; X < EndX; ++X)
			{
				const int32 CellId = GridCache.CoordToId(X, Y, EndZ - 1);
				if (GridCache.GetCellExists(CellId))
				{
					OutCellIds.Add(CellId);
				}
			}
		}
		break;
	}
} 
