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
#include "Engine/NetSerialization.h"
#include "GeometryCollection/GeometryCollectionParticlesData.h"
#include "GridCellTypes.generated.h"


// 前向声明
struct FRealtimeDestructionRequest;

//=========================================================================
// SubCell 配置常量
//=========================================================================

/** 每个轴的 SubCell 划分 - 2x2x2 = 8个子单元 */
inline constexpr int32 SUBCELL_DIVISION = 2;

/** SubCell 总数 */
inline constexpr int32 SUBCELL_COUNT = SUBCELL_DIVISION * SUBCELL_DIVISION * SUBCELL_DIVISION;  // 8

/** SubCell 3D 坐标 -> SubCell ID */
inline constexpr int32 SubCellCoordToId(int32 X, int32 Y, int32 Z)
{
	return Z * (SUBCELL_DIVISION * SUBCELL_DIVISION) + Y * SUBCELL_DIVISION + X;
}

/** SubCell ID -> 3D 坐标 */
inline FIntVector SubCellIdToCoord(int32 SubCellId)
{
	const int32 XY = SUBCELL_DIVISION * SUBCELL_DIVISION;
	const int32 Z = SubCellId / XY;
	const int32 Remainder = SubCellId % XY;
	const int32 Y = Remainder / SUBCELL_DIVISION;
	const int32 X = Remainder % SUBCELL_DIVISION;
	return FIntVector(X, Y, Z);
}

/** 6个方向的偏移量 (+/-X, +/-Y, +/-Z) */
inline constexpr int32 DIRECTION_OFFSETS[6][3] = {
	{-1, 0, 0},  // -X
	{+1, 0, 0},  // +X
	{0, -1, 0},  // -Y
	{0, +1, 0},  // +Y
	{0, 0, -1},  // -Z
	{0, 0, +1},  // +Z
};

// =======================================================
// 底层实用工具
// =======================================================

USTRUCT(BlueprintType)
struct FIntArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> Values;

	void Add(int32 Value) { Values.Add(Value); }
	int32 Num() const { return Values.Num(); }
	void Empty() { Values.Empty(); }
	int32& operator[](int32 Index) { return Values[Index]; }
	const int32& operator[](int32 Index) const { return Values[Index]; }

	// 支持基于范围的 for 循环
	TArray<int32>::RangedForIteratorType begin() { return Values.begin(); }
	TArray<int32>::RangedForIteratorType end() { return Values.end(); }
	TArray<int32>::RangedForConstIteratorType begin() const { return Values.begin(); }
	TArray<int32>::RangedForConstIteratorType end() const { return Values.end(); }
};

/**
 * 用于子单元的有向边界框 (OBB)。
 * 表示世界空间中的一个旋转长方体。
 * 注意：为避免与 UE 的 FOrientedBox 名称冲突而单独定义。
 */
struct FCellOBB
{
	/** 长方体中心（世界空间） */
	FVector Center;

	/** 半边长（局部轴） */
	FVector HalfExtents;

	/** 局部轴（世界空间，归一化的正交向量） */
	FVector AxisX;
	FVector AxisY;
	FVector AxisZ;

	FCellOBB()
		: Center(FVector::ZeroVector)
		, HalfExtents(FVector::ZeroVector)
		, AxisX(FVector::ForwardVector)
		, AxisY(FVector::RightVector)
		, AxisZ(FVector::UpVector)
	{}

	FCellOBB(const FVector& InCenter, const FVector& InHalfExtents, const FQuat& Rotation)
		: Center(InCenter)
		, HalfExtents(InHalfExtents)
	{
		AxisX = Rotation.RotateVector(FVector::ForwardVector);
		AxisY = Rotation.RotateVector(FVector::RightVector);
		AxisZ = Rotation.RotateVector(FVector::UpVector);
	}

	/** 将点变换到 OBB 局部空间。 */
	FVector WorldToLocal(const FVector& WorldPoint) const
	{
		const FVector Delta = WorldPoint - Center;
		return FVector(
			FVector::DotProduct(Delta, AxisX),
			FVector::DotProduct(Delta, AxisY),
			FVector::DotProduct(Delta, AxisZ)
		);
	}

	/** 将局部 OBB 点变换到世界空间。 */
	FVector LocalToWorld(const FVector& LocalPoint) const
	{
		return Center + AxisX * LocalPoint.X + AxisY * LocalPoint.Y + AxisZ * LocalPoint.Z;
	}

	/** 查找 OBB 上或内部距离世界空间点最近的点。 */
	FVector GetClosestPoint(const FVector& WorldPoint) const
	{
		const FVector LocalPoint = WorldToLocal(WorldPoint);
		const FVector ClampedLocal(
			FMath::Clamp(LocalPoint.X, -HalfExtents.X, HalfExtents.X),
			FMath::Clamp(LocalPoint.Y, -HalfExtents.Y, HalfExtents.Y),
			FMath::Clamp(LocalPoint.Z, -HalfExtents.Z, HalfExtents.Z)
		);
		return LocalToWorld(ClampedLocal);
	}
	 
};

/**
 * 用于单元格销毁的形状。
 * 不要与网格工具形状类型混淆。
 */
UENUM(BlueprintType)
enum class ECellDestructionShapeType : uint8
{
	Sphere,     // 球体（爆炸）
	Box,        // 长方体（破墙）
	Cylinder,   // 圆柱体
	Line        // 线（子弹）
};

// =======================================================
// 销毁输入和形状
// =======================================================

USTRUCT(BlueprintType)
struct REALTIMEDESTRUCTION_API FCellDestructionShape
{
	GENERATED_BODY()

	/** 形状类型。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestructionShape")
	ECellDestructionShapeType Type = ECellDestructionShapeType::Sphere;

	/**
	 * 中心点。
	 * 球体/长方体/圆柱体 -> 中心
	 * 线 -> 起点
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestructionShape")
	FVector Center = FVector::ZeroVector;

	/** 球体/圆柱体半径。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestructionShape")
	float Radius = 50.0f;

	/**
	 * 长方体范围（仅长方体）。
	 * 圆柱体使用 Z 值作为高度。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestructionShape")
	FVector BoxExtent = FVector::ZeroVector;

	/** 旋转（长方体/圆柱体）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestructionShape")
	FRotator Rotation = FRotator::ZeroRotator;

	/** 线终点。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestructionShape")
	FVector EndPoint = FVector::ZeroVector;

	/** 线粗细。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestructionShape")
	float LineThickness = 5.0f;

	/** 检查点是否在销毁形状内部。 */
	bool ContainsPoint(const FVector& Point) const;

	/**
	 * 从 FRealtimeDestructionRequest 构建 FCellDestructionShape。
	 * 将 ToolShape 转换为球体/线，并将圆柱体映射为沿 ToolForwardVector 的线。
	 */
	static FCellDestructionShape CreateFromRequest(const FRealtimeDestructionRequest& Request);
};

USTRUCT(BlueprintType)
struct REALTIMEDESTRUCTION_API FQuantizedDestructionInput
{
	GENERATED_BODY()

	/** 销毁形状类型。 */
	UPROPERTY()
	ECellDestructionShapeType Type = ECellDestructionShapeType::Sphere;

	/** 中心（毫米，整数）- 厘米 * 10。 */
	UPROPERTY()
	FIntVector CenterMM = FIntVector::ZeroValue;

	/** 半径（毫米，整数）- 厘米 * 10。 */
	UPROPERTY()
	int32 RadiusMM = 0;

	/** 长方体范围（毫米）。 */
	UPROPERTY()
	FIntVector BoxExtentMM = FIntVector::ZeroValue;

	/** 旋转（0.01度单位，整数）。 */
	UPROPERTY()
	FIntVector RotationCentidegrees = FIntVector::ZeroValue;

	/** 线终点（毫米）。 */
	UPROPERTY()
	FIntVector EndPointMM = FIntVector::ZeroValue;

	/** 线粗细（毫米）。 */
	UPROPERTY()
	int32 LineThicknessMM = 0;

	/** 从浮点值构建量化输入。 */
	static FQuantizedDestructionInput FromDestructionShape(const FCellDestructionShape& Shape);

	/** 恢复为 FCellDestructionShape。 */
	FCellDestructionShape ToDestructionShape() const;

	/** 检查点是否在形状内部（量化值）。 */
	bool ContainsPoint(const FVector& Point) const;

	/**
	 * 检查 OBB（有向边界框）是否与销毁形状相交。
	 * 用于即使在非均匀缩放的网格上也能精确相交。
	 *
	 * @param OBB - 世界空间中的 OBB
	 * @return 是否相交
	 */
	bool IntersectsOBB(const FCellOBB& OBB) const;
};

// =======================================================
// 静态网格布局
// =======================================================

USTRUCT(BlueprintType)
struct REALTIMEDESTRUCTION_API FGridCellLayout
{
	GENERATED_BODY()

	//=========================================================================
	// 网格信息
	//=========================================================================

	/** 网格尺寸（X、Y、Z方向的单元格数量）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GridLayout")
	FIntVector GridSize = FIntVector::ZeroValue;
	
	/** 局部空间中的单元格大小（cm）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GridLayout")
	FVector CellSize = FVector(5.0f, 5.0f, 5.0f);

	/** 网格原点（网格边界最小值）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GridLayout")
	FVector GridOrigin = FVector::ZeroVector;

	/** 网格缩放（构建时的组件缩放）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GridLayout")
	FVector MeshScale = FVector::OneVector;

	//=========================================================================
	// 位域数据（内存优化）
	//=========================================================================

	/** 单元格存在位域（每32个单元格1个uint32）。 */
	UPROPERTY()
	TArray<uint32> CellExistsBits;

	/** 锚点单元格位域（每32个单元格1个uint32）。 */
	UPROPERTY()
	TArray<uint32> CellIsAnchorBits;

	//=========================================================================
	// 稀疏数组数据（仅有效单元格）
	//=========================================================================

	/** 有效单元格ID -> 稀疏索引映射。 */
	UPROPERTY()
	TMap<int32, int32> CellIdToSparseIndex;

	/** 稀疏索引 -> 单元格ID反向映射。 */
	UPROPERTY()
	TArray<int32> SparseIndexToCellId;

	/** 稀疏数组：每个单元格的三角形索引（仅有效单元格）。 */
	UPROPERTY()
	TArray<FIntArray> SparseCellTriangles;

	/** 稀疏数组：每个单元格的邻居ID（仅有效单元格）。 */
	UPROPERTY()
	TArray<FIntArray> SparseCellNeighbors;

	//=========================================================================
	// 缓存的三角形数据（用于运行时体素化）
	//=========================================================================

	/** 从源网格缓存的顶点位置（编辑器时捕获）。 */
	UPROPERTY()
	TArray<FVector> CachedVertices;

	/** 从源网格缓存的三角形索引（编辑器时捕获）。 */
	UPROPERTY()
	TArray<uint32> CachedIndices;

	/** 检查缓存的三角形数据是否可用。 */
	bool HasCachedTriangleData() const
	{
		return CachedVertices.Num() > 0 && CachedIndices.Num() >= 3;
	}

	/** 清除缓存的三角形数据（如果内存是问题，在体素化后调用）。 */
	void ClearCachedTriangleData()
	{
		CachedVertices.Empty();
		CachedIndices.Empty();
	}

	//=========================================================================
	// 位域访问器
	//=========================================================================

	/** 检查单元格是否存在。 */
	FORCEINLINE bool GetCellExists(int32 CellId) const
	{
		const int32 WordIndex = CellId >> 5;  // CellId / 32
		const uint32 BitMask = 1u << (CellId & 31);  // CellId % 32
		return CellExistsBits.IsValidIndex(WordIndex) && (CellExistsBits[WordIndex] & BitMask) != 0;
	}

	/** 设置单元格存在状态。 */
	FORCEINLINE void SetCellExists(int32 CellId, bool bExists)
	{
		const int32 WordIndex = CellId >> 5;
		const uint32 BitMask = 1u << (CellId & 31);
		if (CellExistsBits.IsValidIndex(WordIndex))
		{
			if (bExists)
				CellExistsBits[WordIndex] |= BitMask;
			else
				CellExistsBits[WordIndex] &= ~BitMask;
		}
	}

	/** 检查单元格是否为锚点。 */
	FORCEINLINE bool GetCellIsAnchor(int32 CellId) const
	{
		const int32 WordIndex = CellId >> 5;
		const uint32 BitMask = 1u << (CellId & 31);
		return CellIsAnchorBits.IsValidIndex(WordIndex) && (CellIsAnchorBits[WordIndex] & BitMask) != 0;
	}

	/** 设置锚点标志。 */
	FORCEINLINE void SetCellIsAnchor(int32 CellId, bool bIsAnchor)
	{
		const int32 WordIndex = CellId >> 5;
		const uint32 BitMask = 1u << (CellId & 31);
		if (CellIsAnchorBits.IsValidIndex(WordIndex))
		{
			if (bIsAnchor)
				CellIsAnchorBits[WordIndex] |= BitMask;
			else
				CellIsAnchorBits[WordIndex] &= ~BitMask;
		}
	}

	//=========================================================================
	// 稀疏数组访问器
	//=========================================================================

	/** 获取单元格的三角形索引数组（如果没有则为空）。 */
	const FIntArray& GetCellTriangles(int32 CellId) const
	{
		const int32* SparseIdx = CellIdToSparseIndex.Find(CellId);
		if (SparseIdx && SparseCellTriangles.IsValidIndex(*SparseIdx))
		{
			return SparseCellTriangles[*SparseIdx];
		}
		static const FIntArray EmptyArray;
		return EmptyArray;
	}

	/** 获取单元格的邻居数组（如果没有则为空）。 */
	const FIntArray& GetCellNeighbors(int32 CellId) const
	{
		const int32* SparseIdx = CellIdToSparseIndex.Find(CellId);
		if (SparseIdx && SparseCellNeighbors.IsValidIndex(*SparseIdx))
		{
			return SparseCellNeighbors[*SparseIdx];
		}
		static const FIntArray EmptyArray;
		return EmptyArray;
	}

	/** 获取单元格的可变三角形索引数组。 */
	FIntArray* GetCellTrianglesMutable(int32 CellId)
	{
		const int32* SparseIdx = CellIdToSparseIndex.Find(CellId);
		if (SparseIdx && SparseCellTriangles.IsValidIndex(*SparseIdx))
		{
			return &SparseCellTriangles[*SparseIdx];
		}
		return nullptr;
	}

	/** 获取单元格的可变邻居数组。 */
	FIntArray* GetCellNeighborsMutable(int32 CellId)
	{
		const int32* SparseIdx = CellIdToSparseIndex.Find(CellId);
		if (SparseIdx && SparseCellNeighbors.IsValidIndex(*SparseIdx))
		{
			return &SparseCellNeighbors[*SparseIdx];
		}
		return nullptr;
	}

	/** 注册一个有效单元格（添加到稀疏数组）。 */
	int32 RegisterValidCell(int32 CellId)
	{
		if (CellIdToSparseIndex.Contains(CellId))
		{
			return CellIdToSparseIndex[CellId];
		}

		const int32 SparseIndex = SparseIndexToCellId.Num();
		CellIdToSparseIndex.Add(CellId, SparseIndex);
		SparseIndexToCellId.Add(CellId);
		SparseCellTriangles.AddDefaulted();
		SparseCellNeighbors.AddDefaulted();
		return SparseIndex;
	}

	/** 有效单元格数量。 */
	int32 GetValidCellCount() const
	{
		return SparseIndexToCellId.Num();
	}

	/** 初始化位域（在设置 GridSize 后调用）。 */
	void InitializeBitfields()
	{
		const int32 TotalCells = GetTotalCellCount();
		const int32 RequiredWords = (TotalCells + 31) >> 5;  // ceil(TotalCells / 32)

		CellExistsBits.SetNumZeroed(RequiredWords);
		CellIsAnchorBits.SetNumZeroed(RequiredWords);
	}

	/** 有效单元格ID列表（安全返回空数组）。 */
	const TArray<int32>& GetValidCellIds() const
	{
		return SparseIndexToCellId;
	}

	/** 检查稀疏数组是否有效。 */
	bool HasValidSparseData() const
	{
		return SparseIndexToCellId.Num() > 0 &&
		       SparseCellTriangles.Num() == SparseIndexToCellId.Num() &&
		       SparseCellNeighbors.Num() == SparseIndexToCellId.Num();
	}

	//=========================================================================
	// 辅助函数
	//=========================================================================

	/** 单元格总数。 */
	int32 GetTotalCellCount() const
	{
		return GridSize.X * GridSize.Y * GridSize.Z;
	}

	/** 锚点单元格数量。 */
	int32 GetAnchorCount() const;

	/** 3D 坐标 -> 单元格 ID。 */
	int32 CoordToId(int32 X, int32 Y, int32 Z) const
	{
		return Z * (GridSize.X * GridSize.Y) + Y * GridSize.X + X;
	}

	int32 CoordToId(const FIntVector& Coord) const
	{
		return CoordToId(Coord.X, Coord.Y, Coord.Z);
	}

	/** 单元格 ID -> 3D 坐标。 */
	FIntVector IdToCoord(int32 CellId) const
	{
		const int32 XY = GridSize.X * GridSize.Y;
		const int32 Z = CellId / XY;
		const int32 Remainder = CellId % XY;
		const int32 Y = Remainder / GridSize.X;
		const int32 X = Remainder % GridSize.X;
		return FIntVector(X, Y, Z);
	}

	/** 检查坐标是否有效。 */
	FORCEINLINE bool IsValidCoord(const FIntVector& Coord) const
	{
		return Coord.X >= 0 && Coord.X < GridSize.X &&
		       Coord.Y >= 0 && Coord.Y < GridSize.Y &&
		       Coord.Z >= 0 && Coord.Z < GridSize.Z;
	}

	/** 检查坐标是否有效（int重载）。 */
	FORCEINLINE bool IsValidCoord(int32 X, int32 Y, int32 Z) const
	{
		return X >= 0 && X < GridSize.X &&
		       Y >= 0 && Y < GridSize.Y &&
		       Z >= 0 && Z < GridSize.Z;
	}

	/** 检查单元格ID是否有效。 */
	bool IsValidCellId(int32 CellId) const
	{
		return CellId >= 0 && CellId < GetTotalCellCount();
	}

	/** 世界位置 -> 单元格ID（如果无效则为INDEX_NONE）。 */
	int32 WorldPosToId(const FVector& WorldPos, const FTransform& MeshTransform) const;

	/** 单元格ID -> 世界中心。 */
	FVector IdToWorldCenter(int32 CellId, const FTransform& MeshTransform) const;

	/** 单元格ID -> 局部中心。 */
	FVector IdToLocalCenter(int32 CellId) const;

	/** 单元格ID -> 世界最小点。 */
	FVector IdToWorldMin(int32 CellId, const FTransform& MeshTransform) const;

	/** 单元格ID -> 局部最小点。 */
	FVector IdToLocalMin(int32 CellId) const;

	/** 获取8个单元格顶点（局部空间）。 */
	TArray<FVector> GetCellVertices(int32 CellId) const;

	/** 重置布局。 */
	void Reset();

	/** 验证布局。 */
	bool IsValid() const;

	//=========================================================================
	// SubCell 辅助函数
	//=========================================================================

	/** SubCell 大小（局部空间）。 */
	FVector GetSubCellSize() const
	{
		return CellSize / static_cast<float>(SUBCELL_DIVISION);
	}

	/** SubCell 局部中心（单元格局部空间）。 */
	FVector GetSubCellLocalOffset(int32 SubCellId) const
	{
		const FIntVector SubCoord = SubCellIdToCoord(SubCellId);
		const FVector SubCellSz = GetSubCellSize();
		return FVector(
			(SubCoord.X + 0.5f) * SubCellSz.X,
			(SubCoord.Y + 0.5f) * SubCellSz.Y,
			(SubCoord.Z + 0.5f) * SubCellSz.Z
		);
	}

	/** SubCell 局部中心（网格局部空间）。 */
	FVector GetSubCellLocalCenter(int32 CellId, int32 SubCellId) const
	{
		const FVector CellMin = IdToLocalMin(CellId);
		return CellMin + GetSubCellLocalOffset(SubCellId);
	}
	
	/** SubCell 世界中心。 */
	FVector GetSubCellWorldCenter(int32 CellId, int32 SubCellId, const FTransform& MeshTransform) const
	{
		const FVector LocalCenter = GetSubCellLocalCenter(CellId, SubCellId);
		return MeshTransform.TransformPosition(LocalCenter);
	}

	/**
	 * SubCell 世界 OBB（有向边界框）。
	 * 精确考虑网格旋转和非均匀缩放。
	 */
	FCellOBB GetSubCellWorldOBB(int32 CellId, int32 SubCellId, const FTransform& MeshTransform) const
	{
		const FVector CellMin = IdToLocalMin(CellId);
		const FIntVector SubCoord = SubCellIdToCoord(SubCellId);
		const FVector SubCellSz = GetSubCellSize();

		// 局部空间中的子单元中心
		const FVector LocalMin = CellMin + FVector(
			SubCoord.X * SubCellSz.X,
			SubCoord.Y * SubCellSz.Y,
			SubCoord.Z * SubCellSz.Z
		);
		const FVector LocalCenter = LocalMin + SubCellSz * 0.5f;

		// 变换到世界空间
		const FVector WorldCenter = MeshTransform.TransformPosition(LocalCenter);

		// 应用了缩放的半边长（局部子单元大小 x 变换缩放）
		const FVector TransformScale = MeshTransform.GetScale3D();
		const FVector WorldHalfExtents = SubCellSz * 0.5f * TransformScale;

		// 创建 OBB（仅旋转；缩放已烘焙到 HalfExtents 中）
		return FCellOBB(WorldCenter, WorldHalfExtents, MeshTransform.GetRotation());
	}

	FCellOBB GetCellWorldOBB(int32 CellID, const FTransform& MeshTransform) const
	{
		const FVector CellLocalCenter = IdToLocalCenter(CellID);
		const FVector CellWorldCenter = MeshTransform.TransformPosition(CellLocalCenter);
		const FVector HalfExtents = CellSize * 0.5f;

		FCellOBB CellWorldOBB(CellWorldCenter, HalfExtents, MeshTransform.GetRotation());

		return CellWorldOBB;
	}

	/** 获取 AABB 内的单元格ID。 */
	TArray<int32> GetCellsInAABB(const FBox& WorldAABB, const FTransform& MeshTransform) const;
};

USTRUCT()
struct FSubCell
{
	GENERATED_BODY()

	/**
	 * 位掩码（每个位表示子单元的活动状态）。
	 * 0 = 死亡, 1 = 活动
	 * 8位表示8个子单元。
	 * SubCellId = X + Y * 2 + Z * 4
	 */
	UPROPERTY()
	uint8 Bits = 0xFF;  // 所有子单元都处于活动状态

	bool IsSubCellAlive(int32 SubCellId) const
	{
		return (Bits & (1 << SubCellId)) != 0;
	}

	void DestroySubCell(int32 SubCellId)
	{
		Bits &= ~(1 << SubCellId);
	}

	/** 检查所有子单元是否都已被销毁。 */
	bool IsFullyDestroyed() const
	{
		return Bits == 0;
	}

	/** 重置（所有子单元都处于活动状态）。 */
	void Reset()
	{
		Bits = 0xFF;
	}
};

// =======================================================
// 销毁结果和状态
// =======================================================

USTRUCT(BlueprintType)
struct REALTIMEDESTRUCTION_API FDestructionResult
{
	GENERATED_BODY()

	/** 新销毁的子单元数量。 */
	UPROPERTY()
	int32 DeadSubCellCount = 0;

	/** 受子单元销毁影响的单元格。 */
	UPROPERTY()
	TArray<int32> AffectedCells;

	/** 新销毁的子单元（CellId -> SubCellId 列表）。 */
	UPROPERTY()
	TMap<int32, FIntArray> NewlyDeadSubCells;

	/** 变为已销毁的单元格（所有子单元都已销毁）。 */
	UPROPERTY()
	TArray<int32> NewlyDestroyedCells;

	/** 是否发生任何销毁。 */
	bool HasAnyDestruction() const
	{
		return DeadSubCellCount > 0 || NewlyDestroyedCells.Num() > 0;
	}
};

USTRUCT(BlueprintType)
struct REALTIMEDESTRUCTION_API FDetachedGroupWithSubCell
{
	GENERATED_BODY()

	/** 完全分离的单元格ID（由单元格级BFS确定）。 */
	UPROPERTY()
	TArray<int32> DetachedCellIds;

	/**
	 * 额外包含的子单元（由子单元泛洪确定）。
	 * 键：CellId，值：该单元格包含的 SubCellId 列表。
	 * - 与分离单元格相邻的活动子单元
	 * - 泛洪边界上的死亡子单元
	 */
	UPROPERTY()
	TMap<int32, FIntArray> IncludedSubCells;

	/** 检查组是否为空。 */
	bool IsEmpty() const
	{
		return DetachedCellIds.Num() == 0 && IncludedSubCells.Num() == 0;
	}
};

USTRUCT(BlueprintType)
struct REALTIMEDESTRUCTION_API FCellState
{
	GENERATED_BODY()

	/** 完全销毁的单元格ID集合。 */
	UPROPERTY()
	TSet<int32> DestroyedCells;

	/** 分离的单元格组（尚未作为碎片生成）。 */
	UPROPERTY()
	TArray<FDetachedGroupWithSubCell> DetachedGroups;

	/**
	 * 子单元状态存储。
	 * 被销毁形状触及的单元格会产生死亡的子单元并在此处添加。
	 * 未添加的单元格的所有子单元都处于活动状态。
	 */
	UPROPERTY()
	TMap<int32, FSubCell> SubCellStates;

	/** 检查单元格是否已销毁。 */
	bool IsCellDestroyed(int32 CellId) const
	{
		return DestroyedCells.Contains(CellId);
	}

	/** 检查子单元是否处于活动状态。 */
	bool IsSubCellAlive(int32 CellId, int32 SubCellId) const
	{
		if (DestroyedCells.Contains(CellId))
		{
			return false;
		}

		const FSubCell* SubCellState = SubCellStates.Find(CellId);
		if (SubCellState)
		{
			return SubCellState->IsSubCellAlive(SubCellId);
		}

		// 如果不存在子单元状态，则所有子单元都处于活动状态
		return true;
	}

	/** 检查单元格是否待分离。 */
	bool IsCellDetached(int32 CellId) const
	{
		for (const FDetachedGroupWithSubCell& Group : DetachedGroups)
		{
			if (Group.DetachedCellIds.Contains(CellId))
			{
				return true;
			}
		}
		return false;
	}

	/** 标记单元格为已销毁。 */
	void DestroyCells(const TArray<int32>& CellIds)
	{
		for (int32 CellId : CellIds)
		{
			DestroyedCells.Add(CellId);
		}
	}

	/** 添加分离的组（仅单元格ID，旧版）。 */
	void AddDetachedGroup(const TArray<int32>& CellIds)
	{
		FDetachedGroupWithSubCell Group;
		Group.DetachedCellIds = CellIds;
		DetachedGroups.Add(MoveTemp(Group));
	}

	/** 添加分离的组（带子单元信息）。 */
	void AddDetachedGroup(const FDetachedGroupWithSubCell& Group)
	{
		DetachedGroups.Add(Group);
	}

	/** 添加分离的组（带子单元信息，移动）。 */
	void AddDetachedGroup(FDetachedGroupWithSubCell&& Group)
	{
		DetachedGroups.Add(MoveTemp(Group));
	}

	/** 将分离的组移动到已销毁状态（在碎片生成后调用）。 */
	void MoveDetachedToDestroyed(int32 GroupIndex)
	{
		if (DetachedGroups.IsValidIndex(GroupIndex))
		{
			const FDetachedGroupWithSubCell& Group = DetachedGroups[GroupIndex];

			// DetachedCellIds -> DestroyedCells
			for (int32 CellId : Group.DetachedCellIds)
			{
				DestroyedCells.Add(CellId);
			}

			// IncludedSubCells -> 在 SubCellStates 中标记为死亡
			for (const auto& SubCellPair : Group.IncludedSubCells)
			{
				const int32 CellId = SubCellPair.Key;
				FSubCell& SubCellState = SubCellStates.FindOrAdd(CellId);
				for (int32 SubCellId : SubCellPair.Value.Values)
				{
					SubCellState.DestroySubCell(SubCellId);
				}
			}

			DetachedGroups.RemoveAt(GroupIndex);
		}
	}

	/** 将所有分离的组移动到已销毁状态。 */
	void MoveAllDetachedToDestroyed()
	{
		for (const FDetachedGroupWithSubCell& Group : DetachedGroups)
		{
			// DetachedCellIds -> DestroyedCells
			for (int32 CellId : Group.DetachedCellIds)
			{
				DestroyedCells.Add(CellId);
			}

			// IncludedSubCells -> 在 SubCellStates 中标记为死亡
			for (const auto& SubCellPair : Group.IncludedSubCells)
			{
				const int32 CellId = SubCellPair.Key;
				FSubCell& SubCellState = SubCellStates.FindOrAdd(CellId);
				for (int32 SubCellId : SubCellPair.Value.Values)
				{
					SubCellState.DestroySubCell(SubCellId);
				}
			}
		}
		DetachedGroups.Empty();
	}

	/** 重置状态。 */
	void Reset()
	{
		DestroyedCells.Empty();
		DetachedGroups.Empty();
	}
};

USTRUCT(BlueprintType)
struct REALTIMEDESTRUCTION_API FDetachedDebrisInfo
{
	GENERATED_BODY()

	/** 碎片唯一ID。 */
	UPROPERTY()
	int32 DebrisId = 0;

	/** 包含的单元格ID。 */
	UPROPERTY()
	TArray<int32> CellIds;

	/** 初始位置。 */
	UPROPERTY()
	FVector_NetQuantize InitialLocation;

	/** 初始速度。 */
	UPROPERTY()
	FVector_NetQuantize InitialVelocity;
};

USTRUCT(BlueprintType)
struct REALTIMEDESTRUCTION_API FBatchedDestructionEvent
{
	GENERATED_BODY()

	/** 量化的销毁输入（用于布尔渲染）。 */
	UPROPERTY()
	TArray<FQuantizedDestructionInput> DestructionInputs;

	/** 所有销毁的单元格ID（去重）。 */
	UPROPERTY()
	TArray<int16> DestroyedCellIds;

	/** 分离的碎片。 */
	UPROPERTY()
	TArray<FDetachedDebrisInfo> DetachedDebris;
};

// =======================================================
// BFS 和 SuperCell
// =======================================================

/**
 * 两级分层BFS节点。
 * 表示 SuperCell 或单个单元格的联合类型。
 *
 * - bIsSupercell = true: Id 是 SuperCell ID
 * - bIsSupercell = false: Id 是 Cell ID
 */
struct REALTIMEDESTRUCTION_API FCellNode
{
	/** 节点 ID（SuperCell ID 或 Cell ID）。 */
	int32 Id = INDEX_NONE;

	/** 这是否是一个 SuperCell 节点。 */
	bool bIsSupercell = false;

	FCellNode() = default;

	FCellNode(int32 InId, bool bInIsSupercell)
		: Id(InId), bIsSupercell(bInIsSupercell)
	{}

	/** 创建一个 SuperCell 节点。 */
	static FCellNode MakeSupercell(int32 SupercellId)
	{
		return FCellNode(SupercellId, true);
	}

	/** 创建一个 Cell 节点。 */
	static FCellNode MakeCell(int32 CellId)
	{
		return FCellNode(CellId, false);
	}

	bool IsValid() const { return Id != INDEX_NONE; }

	bool operator==(const FCellNode& Other) const
	{
		return Id == Other.Id && bIsSupercell == Other.bIsSupercell;
	}
};

USTRUCT(BlueprintType)
struct REALTIMEDESTRUCTION_API FSuperCellState
{
	GENERATED_BODY()

	//=========================================================================
	// SuperCell 网格信息
	//=========================================================================

	/** 每个 SuperCell 每轴的单元格数（最大 4x4x4）。 */
	UPROPERTY()
	FIntVector SupercellSize = FIntVector(4, 4, 4);

	/** SuperCell 网格尺寸（X、Y、Z方向的 SuperCell 数量）。 */
	UPROPERTY()
	FIntVector SupercellCount = FIntVector::ZeroValue;

	//=========================================================================
	// SuperCell 状态位域
	//=========================================================================

	/**
	 * 完整状态位域（每64个 SuperCell 1个 uint64）。
	 * 1 = 完整（所有子单元都处于活动状态），0 = 破损
	 */
	UPROPERTY()
	TArray<uint64> IntactBits;

	//=========================================================================
	// Cell <-> SuperCell 映射
	//=========================================================================

	/**
	 * Cell ID -> SuperCell ID 映射。
	 * INDEX_NONE (-1) 表示孤立单元格（未包含在 SuperCell 中）。
	 */
	UPROPERTY()
	TArray<int32> CellToSupercell;

	/**
	 * 孤立单元格ID列表。
	 * 未包含在 SuperCell 中的单元格（网格边缘的剩余单元格）。
	 */
	UPROPERTY()
	TArray<int32> OrphanCellIds;

	// 每个 Supercell 中最初的有效单元格数量（不包括 intact=false 的单元格）
	UPROPERTY()
	TArray<int32> InitialValidCellCounts;

	// 每个 Supercell 中当前被销毁的单元格数量
	UPROPERTY()
	TArray<int32> DestroyedCellCounts;

	//=========================================================================
	// SuperCell 坐标 <-> ID 转换
	//=========================================================================

	/** 3D 坐标 -> SuperCell ID。 */
	FORCEINLINE int32 SupercellCoordToId(int32 X, int32 Y, int32 Z) const
	{
		return Z * (SupercellCount.X * SupercellCount.Y) + Y * SupercellCount.X + X;
	}

	FORCEINLINE int32 SupercellCoordToId(const FIntVector& Coord) const
	{
		return SupercellCoordToId(Coord.X, Coord.Y, Coord.Z);
	}

	/** SuperCell ID -> 3D 坐标。 */
	FORCEINLINE FIntVector SupercellIdToCoord(int32 SupercellId) const
	{
		const int32 XY = SupercellCount.X * SupercellCount.Y;
		const int32 Z = SupercellId / XY;
		const int32 Remainder = SupercellId % XY;
		const int32 Y = Remainder / SupercellCount.X;
		const int32 X = Remainder % SupercellCount.X;
		return FIntVector(X, Y, Z);
	}

	/** SuperCell 总数。 */
	FORCEINLINE int32 GetTotalSupercellCount() const
	{
		return SupercellCount.X * SupercellCount.Y * SupercellCount.Z;
	}

	/** 检查 SuperCell 坐标是否有效。 */
	FORCEINLINE bool IsValidSupercellCoord(const FIntVector& Coord) const
	{
		return Coord.X >= 0 && Coord.X < SupercellCount.X &&
		       Coord.Y >= 0 && Coord.Y < SupercellCount.Y &&
		       Coord.Z >= 0 && Coord.Z < SupercellCount.Z;
	}

	/** 检查 SuperCell ID 是否有效。 */
	FORCEINLINE bool IsValidSupercellId(int32 SupercellId) const
	{
		return SupercellId >= 0 && SupercellId < GetTotalSupercellCount();
	}

	//=========================================================================
	// 完整位域访问器
	//=========================================================================

	/** 检查 SuperCell 是否完整。 */
	FORCEINLINE bool IsSupercellIntact(int32 SupercellId) const
	{
		const int32 WordIndex = SupercellId >> 6;  // SupercellId / 64
		const uint64 BitMask = 1ull << (SupercellId & 63);  // SupercellId % 64
		return IntactBits.IsValidIndex(WordIndex) && (IntactBits[WordIndex] & BitMask) != 0;
	}

	/** 设置 SuperCell 完整状态。 */
	FORCEINLINE void SetSupercellIntact(int32 SupercellId, bool bIntact)
	{
		const int32 WordIndex = SupercellId >> 6;
		const uint64 BitMask = 1ull << (SupercellId & 63);
		if (IntactBits.IsValidIndex(WordIndex))
		{
			if (bIntact)
				IntactBits[WordIndex] |= BitMask;
			else
				IntactBits[WordIndex] &= ~BitMask;
		}
	}

	/** 将 SuperCell 标记为破损。 */
	FORCEINLINE void MarkSupercellBroken(int32 SupercellId)
	{
		SetSupercellIntact(SupercellId, false);
	}

	//=========================================================================
	// Cell <-> SuperCell 关系
	//=========================================================================

	/** 获取单元格的 SuperCell ID（如果是孤立单元格则为 INDEX_NONE）。 */
	FORCEINLINE int32 GetSupercellForCell(int32 CellId) const
	{
		return CellToSupercell.IsValidIndex(CellId) ? CellToSupercell[CellId] : INDEX_NONE;
	}

	/** 检查单元格是否为孤立单元格。 */
	FORCEINLINE bool IsCellOrphan(int32 CellId) const
	{
		return GetSupercellForCell(CellId) == INDEX_NONE;
	}

	/** 单元格坐标 -> SuperCell 坐标。 */
	FORCEINLINE FIntVector CellCoordToSupercellCoord(const FIntVector& CellCoord) const
	{
		return FIntVector(
			CellCoord.X / SupercellSize.X,
			CellCoord.Y / SupercellSize.Y,
			CellCoord.Z / SupercellSize.Z
		);
	}

	/** 检查单元格是否在 SuperCell 边界上（6方向检查）。 */
	bool IsCellOnSupercellBoundary(const FIntVector& CellCoord, const FIntVector& SupercellCoord) const;

	//=========================================================================
	// 迭代 SuperCell 内部的单元格
	//=========================================================================

	/** 构建 SuperCell 中的单元格ID列表。 */
	void GetCellsInSupercell(int32 SupercellId, const FGridCellLayout& GridLayout, TArray<int32>& OutCellIds) const;

	/** 构建 SuperCell 边界上的单元格ID列表（6个面）。 */
	void GetBoundaryCellsOfSupercell(int32 SupercellId, const FGridCellLayout& GridLayout, TArray<int32>& OutCellIds) const;

	//=========================================================================
	// 构建和初始化
	//=========================================================================

	/** 构建 SuperCell 状态（在构建 GridCellLayout 后调用）。 */
	void BuildFromGridLayout(const FGridCellLayout& GridLayout);

	/** 初始化完整位域（将所有 SuperCell 设置为完整）。 */
	void InitializeIntactBits();

	/** 重置状态。 */
	void Reset();

	/** 验证状态。 */
	bool IsValid() const;

	//=========================================================================
	// 分层 BFS 辅助函数
	//=========================================================================

	/**
	 * 检查 SuperCell 是否真正完整（包括子单元状态）。
	 *
	 * 行为取决于 bEnableSubcell：
	 * - bEnableSubcell = true: 每个单元格的每个子单元都必须处于活动状态
	 * - bEnableSubcell = false: DestroyedCells 中不得存在任何单元格
	 *
	 * @param SupercellId - 要检查的 SuperCell ID
	 * @param GridLayout - 网格布局（用于单元格坐标转换）
	 * @param CellState - 单元格状态（销毁/子单元状态）
	 * @param bEnableSubcell - 是否启用子单元模式
	 * @return SuperCell 是否完整
	 */
	bool IsSupercellTrulyIntact(
		int32 SupercellId,
		const FGridCellLayout& GridLayout,
		const FCellState& CellState,
		bool bEnableSubcell) const;

	/**
	 * 更新受已销毁单元格/子单元影响的 SuperCell 状态。
	 *
	 * 在销毁时调用，以将所属的 SuperCell 标记为破损。
	 *
	 * @param AffectedCellIds - 受影响的单元格ID
	 */
	void UpdateSupercellStates(const TArray<int32>& AffectedCellIds);

	/**
	 * 更新单个单元格销毁的 SuperCell 状态。
	 *
	 * @param CellId - 已销毁的单元格ID
	 */
	void OnCellDestroyed(int32 CellId);

	/**
	 * 更新子单元销毁的 SuperCell 状态。
	 * 仅当 bEnableSubcell 为 true 时调用。
	 *
	 * @param CellId - 包含子单元的单元格ID
	 * @param SubCellId - 已销毁的 SubCell ID
	 */
	void OnSubCellDestroyed(int32 CellId, int32 SubCellId);

	/**
	 * 获取给定 SuperCell 方向上的边界单元格ID。
	 *
	 * @param SupercellId - SuperCell ID
	 * @param Direction - 方向 (0:-X, 1:+X, 2:-Y, 3:+Y, 4:-Z, 5:+Z)
	 * @param GridLayout - 网格布局
	 * @param OutCellIds - 边界单元格ID（输出）
	 */
	void GetBoundaryCellsInDirection(
		int32 SupercellId,
		int32 Direction,
		const FGridCellLayout& GridLayout,
		TArray<int32>& OutCellIds) const;
};

struct FConnectivityContext
{
	TArray<uint32> ConnectedCellBits = {};
	TArray<uint32> VisitedSuperCellBits = {};

	TArray<int32> ConnectedCellIds = {};

	TArray<FCellNode> WorkStack = {};

	FConnectivityContext() = default;
	~FConnectivityContext()
	{
		ConnectedCellBits.Empty();
		VisitedSuperCellBits.Empty();
		WorkStack.Empty();
	}

	void Reset(int32 MaxCells, int32 MaxSuperCells)
	{
		// 单元格数量
		const int32 RequiredCellWords = (MaxCells + 31) >> 5;	// 除以 32(2^5)

		// 内存重新分配
		if (ConnectedCellBits.Num() < RequiredCellWords)
		{
			ConnectedCellBits.SetNumUninitialized(RequiredCellWords);
		}

		// 将元素初始化为 0
		FMemory::Memzero(ConnectedCellBits.GetData(), sizeof(uint32) * ConnectedCellBits.Num());

		// Super Cell 数量
		const int32 RequiredSuperCellWords = (MaxSuperCells + 31) >> 5;	// 除以 32(2^5)

		// 内存重新分配
		if (VisitedSuperCellBits.Num() < RequiredSuperCellWords)
		{
			VisitedSuperCellBits.SetNumUninitialized(RequiredSuperCellWords);
		}

		// 将元素初始化为 0
		FMemory::Memzero(VisitedSuperCellBits.GetData(), sizeof(uint32) * VisitedSuperCellBits.Num());

		// 重置堆栈，不释放内存
		WorkStack.Reset();

		// 已连接单元格集合数组的内存管理策略
		// 如果容量超过 8KB 且利用率低于 25%，则重新分配为当前使用量的 2 倍
		int32 CurrentUsage = ConnectedCellIds.Num();			// 当前使用量
		int32 CurrentArrayCapacity = ConnectedCellIds.Max();	// 当前容量
		if (CurrentArrayCapacity > 2048 && CurrentUsage < (CurrentArrayCapacity / 4))
		{
			int32 NewCapacity = CurrentUsage * 2;
			ConnectedCellIds.Empty(NewCapacity);
		}
		else
		{
			ConnectedCellIds.Reset();
		}
	}

	FORCEINLINE bool IsCellConnected(int32 CellId)
	{
		if (CellId < 0)
		{
			return false;
		}

		const int32 WordIndex = CellId >> 5;	// 除以 32
		const uint32 BitMask = 1u << (CellId & 31); // 模 32

		return (ConnectedCellBits[WordIndex] & BitMask) != 0;
	}

	FORCEINLINE void SetCellConnected(int32 CellId)
	{
		if (CellId < 0)
		{
			return;
		}

		const int32 WordIndex = CellId >> 5;	// 除以 32
		const uint32 BitMask = 1u << (CellId & 31); // 模 32

		if (ConnectedCellBits[WordIndex] & BitMask)
		{
			return;
		}

		ConnectedCellIds.Add(CellId);
		
		// 检查访问
		ConnectedCellBits[WordIndex] |= BitMask;
	}

	FORCEINLINE bool IsSuperCellVisited(int32 SuperCellId)
	{
		if (SuperCellId < 0)
		{
			return false;
		}

		const int32 WordIndex = SuperCellId >> 5;	// 除以 32
		const uint32 BitMask = 1u << (SuperCellId & 31); // 模 32

		return (VisitedSuperCellBits[WordIndex] & BitMask) != 0;
	}

	FORCEINLINE void SetSuperCellVisited(int32 SuperCellId)
	{
		if (SuperCellId < 0)
		{
			return;
		}

		const int32 WordIndex = SuperCellId >> 5;	// 除以 32
		const uint32 BitMask = 1u << (SuperCellId & 31); // 模 32

		// 检查访问
		VisitedSuperCellBits[WordIndex] |= BitMask;
	}

	FORCEINLINE bool CheckAndSetCell(int32 CellId)
	{
		if (CellId < 0)
		{
			return true;
		}
		
		const int32 WordIndex = CellId >> 5;	// 除以 32
		const uint32 BitMask = 1u << (CellId & 31); // 模 32

		// 已访问的单元格
		if (ConnectedCellBits[WordIndex] & BitMask)
		{
			return true;
		}

		// 检查访问
		ConnectedCellBits[WordIndex] |= BitMask;
		
		return false;
	}

	FORCEINLINE bool CheckAndSetSuperCell(int32 SuperCellId)
	{
		if (SuperCellId < 0)
		{
			return true;
		}
		
		const int32 WordIndex = SuperCellId >> 5;	// 除以 32
		const uint32 BitMask = 1u << (SuperCellId & 31); // 模 32

		// 已访问的单元格
		if (VisitedSuperCellBits[WordIndex] & BitMask)
		{
			return true;
		}

		// 检查访问
		VisitedSuperCellBits[WordIndex] |= BitMask;
		return false;
	}

	void CollectConnectedCells(TSet<int32>& OutConnectedCells)
	{
		const int32 NumWord = ConnectedCellBits.Num();
		for (int32 i = 0 ; i < NumWord; i++)
		{
			uint32 Word = ConnectedCellBits[i];
			if (Word == 0)
			{
				continue;
			}

			for (int32 Bit = 0 ; Bit < 32; Bit++)
			{
				if (Word & (1u << Bit))
				{
					OutConnectedCells.Add((i << 5) | Bit);
				}
			}
		}
	}
};