// Copyright (c) 2026 LazyDevelopers <lazydeveloper24@gmail.com>. All rights reserved.
// This plugin is distributed under the Fab Standard License.
//
// This product was independently developed by us while participating in the Epic Project, a developer-support
// program of the KRAFTON JUNGLE GameTech Lab. All rights, title, and interest in and to the product are exclusively
// vested in us. Krafton, Inc. was not involved in its development and distribution and disclaims all representations
// and warranties, express or implied, and assumes no responsibility or liability for any consequences arising from
// the use of this product.

#include "StructuralIntegrity/CellDestructionSystem.h"
#include "Containers/Queue.h"
#include "StructuralIntegrity/SubCellProcessor.h"
//=============================================================================
// FCellDestructionSystem - SubCell level API
//=============================================================================

FDestructionResult FCellDestructionSystem::ProcessCellDestructionSubCellLevel(
	const FGridCellLayout& GridLayout,
	const FQuantizedDestructionInput& Shape,
	const FTransform& MeshTransform,
	FCellState& InOutCellState)
{
	FDestructionResult Result;

	if (!GridLayout.IsValid())
	{
		return Result;
	}

	// 1.通过SubCellProcessor进行SubCell销毁
	TArray<int32> AffectedCells;
	TMap<int32, TArray<int32>> NewlyDeadSubCells;

	FSubCellProcessor::ProcessSubCellDestruction(
		Shape,
		MeshTransform,
		GridLayout,
		InOutCellState,
		AffectedCells,
		&NewlyDeadSubCells
	);

	// 2. 收集结果
	Result.AffectedCells = MoveTemp(AffectedCells);

	// NewlyDeadSubCells (TMap<int32, TArray<int32>>) -> FDestructionResult.NewlyDeadSubCells (TMap<int32, FIntArray>)
	for (auto& Pair : NewlyDeadSubCells)
	{
		FIntArray SubCellArray;
		SubCellArray.Values = MoveTemp(Pair.Value);
		Result.DeadSubCellCount += SubCellArray.Num();
		Result.NewlyDeadSubCells.Add(Pair.Key, MoveTemp(SubCellArray));
	}

	// 3. 收集完全被摧毁的单元（已在SubCellProcessor中添加到DestroyedCells）
	for (int32 CellId : Result.AffectedCells)
	{
		if (InOutCellState.DestroyedCells.Contains(CellId))
		{
			Result.NewlyDestroyedCells.Add(CellId);
		}
	}

	return Result;
}

//=============================================================================
// FCellDestructionSystem - Cell destruction evaluation (legacy cell-level API)
//=============================================================================

TArray<int32> FCellDestructionSystem::ProcessCellDestruction(
	const FGridCellLayout& GridLayout,
	const FQuantizedDestructionInput& Shape,
	const FTransform& MeshTransform,
	const TSet<int32>& DestroyedCells)
{
	TArray<int32> NewlyDestroyed;

	for (int32 CellId = 0; CellId < GridLayout.GetTotalCellCount(); CellId++)
	{
		// 跳过已被摧毁或不存在的单元
		if (!GridLayout.GetCellExists(CellId) || DestroyedCells.Contains(CellId))
		{
			continue;
		}

		if (IsCellDestroyed(GridLayout, CellId, Shape, MeshTransform))
		{
			NewlyDestroyed.Add(CellId);
		}
	}

	return NewlyDestroyed;
}

FDestructionResult FCellDestructionSystem::ProcessCellDestruction(
	const FGridCellLayout& GridLayout,
	const FQuantizedDestructionInput& Shape,
	const FTransform& MeshTransform,
	FCellState& InOutCellState)
{
	FDestructionResult Result;
	Result.NewlyDestroyedCells = ProcessCellDestruction(GridLayout, Shape, MeshTransform, InOutCellState.DestroyedCells);
	InOutCellState.DestroyCells(Result.NewlyDestroyedCells);
	return Result;
}

bool FCellDestructionSystem::IsCellDestroyed(
	const FGridCellLayout& GridLayout,
	int32 CellId,
	const FQuantizedDestructionInput& Shape,
	const FTransform& MeshTransform)
{
	// 阶段1：中心点测试（快速）
	const FVector WorldCenter = GridLayout.IdToWorldCenter(CellId, MeshTransform);
	if (Shape.ContainsPoint(WorldCenter))
	{
		return true;
	}

	// 阶段2：多数顶点测试（边缘情况）
	const TArray<FVector> LocalVertices = GridLayout.GetCellVertices(CellId);
	int32 DestroyedVertices = 0;

	for (const FVector& LocalVertex : LocalVertices)
	{
		const FVector WorldVertex = MeshTransform.TransformPosition(LocalVertex);
		if (Shape.ContainsPoint(WorldVertex))
		{
			// 当达到多数（4）时立即返回
			if (++DestroyedVertices >= 4)
			{
				return true;
			}
		}
	}

	return false;
}

//=============================================================================
// FCellDestructionSystem - 结构完整性检查
//=============================================================================

TSet<int32> FCellDestructionSystem::FindDisconnectedCells(
	const FGridCellLayout& GridLayout,
	FSuperCellState& SupercellState,
	const FCellState& CellState,
	bool bEnableSupercell,
	bool bEnableSubcell,
	FConnectivityContext& Context)
{
	if (bEnableSupercell)
	{
		return FindDisconnectedCellsHierarchicalLevel(
			GridLayout,
			SupercellState,
			CellState,
			bEnableSubcell,
			Context);
	}
	if (bEnableSubcell)
	{
		return FindDisconnectedCellsSubCellLevel(
			GridLayout,
			CellState);
	}
	return FindDisconnectedCellsCellLevel(GridLayout, CellState.DestroyedCells);
}

TSet<int32> FCellDestructionSystem::FindDisconnectedCellsCellLevel(
	const FGridCellLayout& GridLayout,
	const TSet<int32>& DestroyedCells)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CellStructure_FindDisconnectedCellsCellLevel)
	TSet<int32> Connected;
	TQueue<int32> Queue;

	// 1. 从锚点开始BFS
	for (int32 CellId = 0; CellId < GridLayout.GetTotalCellCount(); CellId++)
	{
		if (GridLayout.GetCellExists(CellId) &&
		    GridLayout.GetCellIsAnchor(CellId) &&
		    !DestroyedCells.Contains(CellId))
		{
			Queue.Enqueue(CellId);
			Connected.Add(CellId);
		}
	}

	// 2. BFS遍历
	while (!Queue.IsEmpty())
	{
		int32 Current;
		Queue.Dequeue(Current);

		for (int32 Neighbor : GridLayout.GetCellNeighbors(Current))
		{
			if (!DestroyedCells.Contains(Neighbor) &&
			    !Connected.Contains(Neighbor))
			{
				Connected.Add(Neighbor);
				Queue.Enqueue(Neighbor);
			}
		}
	}

	// 3. 未连接的单元被分离
	TSet<int32> Disconnected;
	int32 ValidCellCount = 0;
	int32 AnchorCount = 0;
	for (int32 CellId = 0; CellId < GridLayout.GetTotalCellCount(); CellId++)
	{
		if (GridLayout.GetCellExists(CellId))
		{
			ValidCellCount++;
			if (GridLayout.GetCellIsAnchor(CellId)) AnchorCount++;

			if (!DestroyedCells.Contains(CellId) &&
			    !Connected.Contains(CellId))
			{
				Disconnected.Add(CellId);
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("FindDisconnectedCellsCellLevel: Valid=%d, Anchor=%d, Destroyed=%d, Connected=%d, Disconnected=%d"),
		ValidCellCount, AnchorCount, DestroyedCells.Num(), Connected.Num(), Disconnected.Num());

	return Disconnected;
}

TArray<TArray<int32>> FCellDestructionSystem::GroupDetachedCells(
	const FGridCellLayout& GridLayout,
	const TSet<int32>& DisconnectedCells,
	const TSet<int32>& DestroyedCells)
{
	TArray<TArray<int32>> Groups;
	TSet<int32> Visited;

	//=========================================================================
	// 阶段1：通过BFS对断开连接的单元进行分组
	//=========================================================================
	for (int32 StartCell : DisconnectedCells)
	{
		if (Visited.Contains(StartCell))
		{
			continue;
		}

		// 通过BFS找到一个已连接的已分离单元组
		TArray<int32> Group;
		TQueue<int32> Queue;

		Queue.Enqueue(StartCell);
		Visited.Add(StartCell);

		while (!Queue.IsEmpty())
		{
			int32 Current;
			Queue.Dequeue(Current);
			Group.Add(Current);

			for (int32 Neighbor : GridLayout.GetCellNeighbors(Current))
			{
				if (DisconnectedCells.Contains(Neighbor) &&
				    !Visited.Contains(Neighbor))
				{
					Visited.Add(Neighbor);
					Queue.Enqueue(Neighbor);
				}
			}
		}

		Groups.Add(MoveTemp(Group));
	}
	
	return Groups;
}

//=============================================================================
// FCellDestructionSystem - 实用工具
//=============================================================================

FVector FCellDestructionSystem::CalculateGroupCenter(
	const FGridCellLayout& GridLayout,
	const TArray<int32>& CellIds,
	const FTransform& MeshTransform)
{
	if (CellIds.Num() == 0)
	{
		return FVector::ZeroVector;
	}

	FVector Sum = FVector::ZeroVector;
	for (int32 CellId : CellIds)
	{
		Sum += GridLayout.IdToWorldCenter(CellId, MeshTransform);
	}

	return Sum / CellIds.Num();
}

FVector FCellDestructionSystem::CalculateDebrisVelocity(
	const FVector& DebrisCenter,
	const TArray<FQuantizedDestructionInput>& DestructionInputs,
	float BaseSpeed)
{
	if (DestructionInputs.Num() == 0)
	{
		return FVector::ZeroVector;
	}

	// 找到最近的破坏输入
	float MinDistSq = MAX_FLT;
	FVector ClosestCenter = FVector::ZeroVector;

	for (const auto& Input : DestructionInputs)
	{
		const FVector Center = FVector(Input.CenterMM.X, Input.CenterMM.Y, Input.CenterMM.Z) * 0.1f;
		const float DistSq = FVector::DistSquared(DebrisCenter, Center);

		if (DistSq < MinDistSq)
		{
			MinDistSq = DistSq;
			ClosestCenter = Center;
		}
	}

	// 爆炸方向的速度
	const FVector Direction = (DebrisCenter - ClosestCenter).GetSafeNormal();
	return Direction * BaseSpeed;
}

bool FCellDestructionSystem::IsBoundaryCell(
	const FGridCellLayout& GridLayout,
	int32 CellId,
	const TSet<int32>& DestroyedCells)
{
	for (int32 Neighbor : GridLayout.GetCellNeighbors(CellId))
	{
		if (DestroyedCells.Contains(Neighbor))
		{
			return true;  // 邻近一个被摧毁的单元=边界
		}
	}
	return false;
}

//=============================================================================
// FDestructionBatchProcessor
//=============================================================================

FDestructionBatchProcessor::FDestructionBatchProcessor()
	: AccumulatedTime(0.0f)
	, LayoutPtr(nullptr)
	, CellStatePtr(nullptr)
	, MeshTransform(FTransform::Identity)
	, DebrisIdCounter(0)
{
}

void FDestructionBatchProcessor::QueueDestruction(const FCellDestructionShape& Shape)
{
	// 存储量化数据
	PendingDestructions.Add(FQuantizedDestructionInput::FromDestructionShape(Shape));
}

bool FDestructionBatchProcessor::Tick(float DeltaTime)
{
	AccumulatedTime += DeltaTime;

	if (AccumulatedTime >= BatchInterval && PendingDestructions.Num() > 0)
	{
		AccumulatedTime = 0.0f;
		ProcessBatch();
		return true;
	}

	return false;
}

void FDestructionBatchProcessor::FlushQueue()
{
	if (PendingDestructions.Num() > 0)
	{
		ProcessBatch();
		AccumulatedTime = 0.0f;
	}
}

void FDestructionBatchProcessor::SetContext(
	const FGridCellLayout* InLayout,
	FCellState* InCellState,
	const FTransform& InMeshTransform)
{
	LayoutPtr = InLayout;
	CellStatePtr = InCellState;
	MeshTransform = InMeshTransform;
}

void FDestructionBatchProcessor::ProcessBatch()
{
	if (!LayoutPtr || !CellStatePtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("FDestructionBatchProcessor: Context not set"));
		PendingDestructions.Empty();
		return;
	}

	// 初始化结果
	LastBatchResult = FBatchedDestructionEvent();
	LastBatchResult.DestructionInputs = PendingDestructions;

	//=====================================================
	// 阶段1：评估所有破坏输入的单元
	//=====================================================
	TSet<int32> NewlyDestroyed;

	for (const auto& Input : PendingDestructions)
	{
		TArray<int32> Cells = FCellDestructionSystem::ProcessCellDestruction(
			*LayoutPtr, Input, MeshTransform, CellStatePtr->DestroyedCells);

		for (int32 CellId : Cells)
		{
			NewlyDestroyed.Add(CellId);
		}
	}

	if (NewlyDestroyed.Num() == 0)
	{
		PendingDestructions.Empty();
		return;
	}

	//=====================================================
	// 阶段2：更新单元状态
	//=====================================================
	for (int32 CellId : NewlyDestroyed)
	{
		CellStatePtr->DestroyedCells.Add(CellId);
	}

	//=====================================================
	// 阶段3：运行一次BFS（批处理的核心）
	//=====================================================
	TSet<int32> Disconnected = FCellDestructionSystem::FindDisconnectedCellsCellLevel(
		*LayoutPtr, CellStatePtr->DestroyedCells);

	TArray<TArray<int32>> DetachedGroups = FCellDestructionSystem::GroupDetachedCells(
		*LayoutPtr, Disconnected, CellStatePtr->DestroyedCells);

	//=====================================================
	// 阶段4：同时销毁分离的单元
	//=====================================================
	for (const auto& Group : DetachedGroups)
	{
		for (int32 CellId : Group)
		{
			CellStatePtr->DestroyedCells.Add(CellId);
		}
	}

	//=====================================================
	// 阶段5：创建事件
	//=====================================================
	for (int32 CellId : NewlyDestroyed)
	{
		LastBatchResult.DestroyedCellIds.Add((int16)CellId);
	}

	// 创建碎片信息
	for (const auto& Group : DetachedGroups)
	{
		FDetachedDebrisInfo DebrisInfo;
		DebrisInfo.DebrisId = ++DebrisIdCounter;

		for (int32 CellId : Group)
		{
			DebrisInfo.CellIds.Add((int16)CellId);
			LastBatchResult.DestroyedCellIds.Add((int16)CellId);
		}

		DebrisInfo.InitialLocation = FCellDestructionSystem::CalculateGroupCenter(
			*LayoutPtr, Group, MeshTransform);

		DebrisInfo.InitialVelocity = FCellDestructionSystem::CalculateDebrisVelocity(
			DebrisInfo.InitialLocation, PendingDestructions);

		LastBatchResult.DetachedDebris.Add(DebrisInfo);
	}

	// 清空队列
	PendingDestructions.Empty();

	UE_LOG(LogTemp, Log, TEXT("FDestructionBatchProcessor: Processed %d destroyed cells, %d debris groups"),
		LastBatchResult.DestroyedCellIds.Num(), LastBatchResult.DetachedDebris.Num());
}

//=============================================================================
// FCellDestructionSystem - Subcell-level connectivity check (2x2x2 optimization)
//=============================================================================

namespace SubCellBFSHelper
{
	/**
	 * Boundary subcell pair table (2x2x2 only).
	 * Four pairs per direction: (current cell subcell, neighbor cell subcell).
	 *
	 * SubCell layout:
	 *   Z=0: 0(0,0,0), 1(1,0,0), 2(0,1,0), 3(1,1,0)
	 *   Z=1: 4(0,0,1), 5(1,0,1), 6(0,1,1), 7(1,1,1)
	 */
	struct FBoundarySubCellPair
	{
		int32 Current;   // Boundary subcell in the current cell
		int32 Neighbor;  // Corresponding subcell in the neighbor cell
	};

	// +X方向：X=1 (1,3,5,7) -> 邻居X=0 (0,2,4,6)
	inline constexpr FBoundarySubCellPair BOUNDARY_PAIRS_POS_X[4] = {
		{1, 0}, {3, 2}, {5, 4}, {7, 6}
	};

	// -X方向：X=0 (0,2,4,6) -> 邻居X=1 (1,3,5,7)
	inline constexpr FBoundarySubCellPair BOUNDARY_PAIRS_NEG_X[4] = {
		{0, 1}, {2, 3}, {4, 5}, {6, 7}
	};

	// +Y方向：Y=1 (2,3,6,7) -> 邻居Y=0 (0,1,4,5)
	inline constexpr FBoundarySubCellPair BOUNDARY_PAIRS_POS_Y[4] = {
		{2, 0}, {3, 1}, {6, 4}, {7, 5}
	};

	// -Y方向：Y=0 (0,1,4,5) -> 邻居Y=1 (2,3,6,7)
	inline constexpr FBoundarySubCellPair BOUNDARY_PAIRS_NEG_Y[4] = {
		{0, 2}, {1, 3}, {4, 6}, {5, 7}
	};

	// +Z方向：Z=1 (4,5,6,7) -> 邻居Z=0 (0,1,2,3)
	inline constexpr FBoundarySubCellPair BOUNDARY_PAIRS_POS_Z[4] = {
		{4, 0}, {5, 1}, {6, 2}, {7, 3}
	};

	// -Z方向：Z=0 (0,1,2,3) -> 邻居Z=1 (4,5,6,7)
	inline constexpr FBoundarySubCellPair BOUNDARY_PAIRS_NEG_Z[4] = {
		{0, 4}, {1, 5}, {2, 6}, {3, 7}
	};

	/**
	 * 返回一个方向的边界子单元对数组。
	 * @param Direction - 0:-X, 1:+X, 2:-Y, 3:+Y, 4:-Z, 5:+Z
	 */
	inline const FBoundarySubCellPair* GetBoundaryPairs(int32 Direction)
	{
		switch (Direction)
		{
		case 0: return BOUNDARY_PAIRS_NEG_X;
		case 1: return BOUNDARY_PAIRS_POS_X;
		case 2: return BOUNDARY_PAIRS_NEG_Y;
		case 3: return BOUNDARY_PAIRS_POS_Y;
		case 4: return BOUNDARY_PAIRS_NEG_Z;
		case 5: return BOUNDARY_PAIRS_POS_Z;
		default: return nullptr;
		}
	}

	/**
	 * 检查两个单元之间是否存在任何连接的边界子单元对。
	 * @param Direction - 从CellA到CellB的方向(0-5)
	 * @return 如果任何边界对在两侧都存在，则为True
	 */
	bool HasConnectedBoundary(
		int32 CellA,
		int32 CellB,
		int32 Direction,
		const FCellState& CellState)
	{
		const FBoundarySubCellPair* Pairs = GetBoundaryPairs(Direction);
		if (!Pairs)
		{
			return false;
		}

		for (int32 i = 0; i < 4; ++i)
		{
			if (CellState.IsSubCellAlive(CellA, Pairs[i].Current) &&
				CellState.IsSubCellAlive(CellB, Pairs[i].Neighbor))
			{
				return true;  // 找到连接对
			}
		}

		return false;  // 无连接
	}

	/**
	 * 检查一个单元格是否包含任何存活的子单元格。
	 */
	bool HasAliveSubCell(int32 CellId, const FCellState& CellState)
	{
		if (CellState.DestroyedCells.Contains(CellId))
		{
			return false;
		}

		const FSubCell* SubCellState = CellState.SubCellStates.Find(CellId);
		if (!SubCellState)
		{
			return true;  // 如果没有状态，则所有子单元都存活
		}

		return !SubCellState->IsFullyDestroyed();
	}

	/**
	 * Check anchor reachability via cell-level BFS (2x2x2 optimization).
	 *
	 * In 2x2x2, all subcells within a cell are connected, so we traverse at the cell level
	 * and only check boundary connectivity at the subcell level.
	 *
	 * @param GridLayout - grid layout
	 * @param CellState - cell state
	 * @param StartCellId - start cell
	 * @param ConfirmedConnected - cells already confirmed as connected
	 * @param OutVisitedCells - visited cell set (output)
	 * @return Whether an anchor is reachable
	 */
	bool PerformSubCellBFS(
		const FGridCellLayout& GridLayout,
		const FCellState& CellState,
		int32 StartCellId,
		const TSet<int32>& ConfirmedConnected,
		TSet<int32>& OutVisitedCells)
	{
		OutVisitedCells.Reset();

		// 检查起始单元格是否有任何存活的子单元格
		if (!HasAliveSubCell(StartCellId, CellState))
		{
			return false;
		}

		// 单元格级别的BFS
		TQueue<int32> CellQueue;
		TSet<int32> VisitedCells;

		CellQueue.Enqueue(StartCellId);
		VisitedCells.Add(StartCellId);
		OutVisitedCells.Add(StartCellId);

		while (!CellQueue.IsEmpty())
		{
			int32 CurrCellId;
			CellQueue.Dequeue(CurrCellId);

			// 检查是否到达锚点单元格
			if (GridLayout.GetCellIsAnchor(CurrCellId))
			{
				return true;
			}

			// 到达一个已经被确认为连接的单元格
			if (ConfirmedConnected.Contains(CurrCellId))
			{
				return true;
			}

			// 探索6个方向的邻居单元格
			const FIntVector CurrCoord = GridLayout.IdToCoord(CurrCellId);

			for (int32 Dir = 0; Dir < 6; ++Dir)
			{
				const FIntVector NeighborCoord = CurrCoord + FIntVector(
					DIRECTION_OFFSETS[Dir][0],
					DIRECTION_OFFSETS[Dir][1],
					DIRECTION_OFFSETS[Dir][2]
				);

				if (!GridLayout.IsValidCoord(NeighborCoord))
				{
					continue;
				}

				const int32 NeighborCellId = GridLayout.CoordToId(NeighborCoord);

				// 跳过已经访问过或无效的单元格
				if (VisitedCells.Contains(NeighborCellId))
				{
					continue;
				}

				if (!GridLayout.GetCellExists(NeighborCellId))
				{
					continue;
				}

				if (CellState.DestroyedCells.Contains(NeighborCellId))
				{
					continue;
				}

				// 检查边界子单元格的连通性
				if (HasConnectedBoundary(CurrCellId, NeighborCellId, Dir, CellState))
				{
					VisitedCells.Add(NeighborCellId);
					CellQueue.Enqueue(NeighborCellId);
					OutVisitedCells.Add(NeighborCellId);
				}
			}
		}

		// 未能到达锚点
		return false;
	}

	/**
	 * Subcell internal adjacency table (2x2x2 only, 6 directions).
	 * For each subcell, adjacent subcell IDs in 6 directions (-1 if none).
	 * Order: -X, +X, -Y, +Y, -Z, +Z
	 */
	inline constexpr int32 SUBCELL_ADJACENCY[8][6] = {
		// SubCell 0 (0,0,0): -X=none, +X=1, -Y=none, +Y=2, -Z=none, +Z=4
		{-1, 1, -1, 2, -1, 4},
		// SubCell 1 (1,0,0): -X=0, +X=none, -Y=none, +Y=3, -Z=none, +Z=5
		{0, -1, -1, 3, -1, 5},
		// SubCell 2 (0,1,0): -X=none, +X=3, -Y=0, +Y=none, -Z=none, +Z=6
		{-1, 3, 0, -1, -1, 6},
		// SubCell 3 (1,1,0): -X=2, +X=none, -Y=1, +Y=none, -Z=none, +Z=7
		{2, -1, 1, -1, -1, 7},
		// SubCell 4 (0,0,1): -X=none, +X=5, -Y=none, +Y=6, -Z=0, +Z=none
		{-1, 5, -1, 6, 0, -1},
		// SubCell 5 (1,0,1): -X=4, +X=none, -Y=none, +Y=7, -Z=1, +Z=none
		{4, -1, -1, 7, 1, -1},
		// SubCell 6 (0,1,1): -X=none, +X=7, -Y=4, +Y=none, -Z=2, +Z=none
		{-1, 7, 4, -1, 2, -1},
		// SubCell 7 (1,1,1): -X=6, +X=none, -Y=5, +Y=none, -Z=3, +Z=none
		{6, -1, 5, -1, 3, -1},
	};

	/**
	 * 返回相反的方向。
	 * 0(-X) <-> 1(+X), 2(-Y) <-> 3(+Y), 4(-Z) <-> 5(+Z)
	 */
	inline constexpr int32 GetOppositeDirection(int32 Direction)
	{
		return Direction ^ 1;  // 0<->1, 2<->3, 4<->5
	}

	/**
	 * 返回一个方向的边界子单元ID（4个条目）。
	 * @param Direction - 0:-X, 1:+X, 2:-Y, 3:+Y, 4:-Z, 5:+Z
	 */
	inline void GetBoundarySubCellIds(int32 Direction, int32 OutIds[4])
	{
		const FBoundarySubCellPair* Pairs = GetBoundaryPairs(Direction);
		if (Pairs)
		{
			for (int32 i = 0; i < 4; ++i)
			{
				OutIds[i] = Pairs[i].Current;
			}
		}
	}

	/**
	 * 从一个分离的单元格边界向一个连接的单元格泛洪子单元格。
	 * 从边界子单元格开始，直到碰到死亡的子单元格为止。
	 *
	 * @param CellState - 单元格状态
	 * @param ConnectedCellId - 连接的单元格ID
	 * @param DirectionFromDetached - 从分离到连接的方向（0-5）
	 * @return 泛洪的子单元格ID列表
	 */
	TArray<int32> FloodSubCellsFromBoundary(
		const FCellState& CellState,
		int32 ConnectedCellId,
		int32 DirectionFromDetached)
	{
		TArray<int32> Result;

		// 分离->连接方向的相反方向=与分离单元格接触的面
		const int32 BoundaryDirection = GetOppositeDirection(DirectionFromDetached);

		// 获取边界子单元格ID
		int32 BoundarySubCellIds[4];
		GetBoundarySubCellIds(BoundaryDirection, BoundarySubCellIds);

		// BFS数据结构
		TSet<int32> Visited;
		TQueue<int32> Queue;

		// 将边界子单元格添加为起点
		for (int32 i = 0; i < 4; ++i)
		{
			const int32 SubCellId = BoundarySubCellIds[i];
			if (!Visited.Contains(SubCellId))
			{
				Visited.Add(SubCellId);
				Queue.Enqueue(SubCellId);
			}
		}

		// BFS遍历
		while (!Queue.IsEmpty())
		{
			int32 CurrentSubCellId;
			Queue.Dequeue(CurrentSubCellId);

			const bool bIsAlive = CellState.IsSubCellAlive(ConnectedCellId, CurrentSubCellId);

			// 添加到结果（存活或死亡）
			Result.Add(CurrentSubCellId);

			// 如果子单元格已死亡，则停止扩展（充当边界）
			if (!bIsAlive)
			{
				continue;
			}

			// 如果子单元格存活，则扩展到相邻的子单元格
			for (int32 Dir = 0; Dir < 6; ++Dir)
			{
				const int32 NeighborSubCellId = SUBCELL_ADJACENCY[CurrentSubCellId][Dir];

				// 跳过无效或已访问的子单元格
				if (NeighborSubCellId < 0 || Visited.Contains(NeighborSubCellId))
				{
					continue;
				}

				Visited.Add(NeighborSubCellId);
				Queue.Enqueue(NeighborSubCellId);
			}
		}

		return Result;
	}

	/**
	 * 分离组的边界单元格信息。
	 */
	struct FBoundaryCellInfo
	{
		int32 BoundaryCellId = INDEX_NONE;
		TArray<TPair<int32, int32>> AdjacentConnectedCells;  // (CellId, Direction)
	};

	/**
	 * 从一个分离的组中提取边界单元格（包括相邻的连接单元格）。
	 */
	TArray<FBoundaryCellInfo> GetGroupBoundaryCellsWithAdjacency(
		const FGridCellLayout& GridLayout,
		const TArray<int32>& GroupCellIds,
		const FCellState& CellState)
	{
		TArray<FBoundaryCellInfo> Result;

		// 将组单元格转换为集合以进行快速查找
		TSet<int32> GroupCellSet;
		GroupCellSet.Reserve(GroupCellIds.Num());
		for (int32 CellId : GroupCellIds)
		{
			GroupCellSet.Add(CellId);
		}

		// 确定每个组单元格的边界状态
		for (int32 CellId : GroupCellIds)
		{
			FBoundaryCellInfo Info;
			Info.BoundaryCellId = CellId;

			const FIntVector CellCoord = GridLayout.IdToCoord(CellId);

			// 检查6个方向的邻居
			for (int32 Dir = 0; Dir < 6; ++Dir)
			{
				const FIntVector NeighborCoord = CellCoord + FIntVector(
					DIRECTION_OFFSETS[Dir][0],
					DIRECTION_OFFSETS[Dir][1],
					DIRECTION_OFFSETS[Dir][2]
				);

				// 跳过无效坐标
				if (!GridLayout.IsValidCoord(NeighborCoord))
				{
					continue;
				}

				const int32 NeighborCellId = GridLayout.CoordToId(NeighborCoord);

				// 跳过组内的单元格
				if (GroupCellSet.Contains(NeighborCellId))
				{
					continue;
				}

				// 跳过不存在的单元格
				if (!GridLayout.GetCellExists(NeighborCellId))
				{
					continue;
				}

				// 跳过被摧毁的单元格（只考虑连接的单元格）
				if (CellState.DestroyedCells.Contains(NeighborCellId))
				{
					continue;
				}

				// 找到一个连接的单元格 -> 添加到邻接列表
				Info.AdjacentConnectedCells.Add(TPair<int32, int32>(NeighborCellId, Dir));
			}

			// 如果存在任何相邻的连接单元格，则它是一个边界单元格
			if (Info.AdjacentConnectedCells.Num() > 0)
			{
				Result.Add(MoveTemp(Info));
			}
		}

		return Result;
	}
}

TSet<int32> FCellDestructionSystem::FindDisconnectedCellsSubCellLevel(
	const FGridCellLayout& GridLayout,
	const FCellState& CellState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CellStructure_FindDisconnectedCellsSubCellLevel);
	using namespace SubCellBFSHelper;

	TSet<int32> Connected;

	// 1. 从所有锚点单元格开始BFS
	TQueue<int32> Queue;
	for (int32 CellId = 0; CellId < GridLayout.GetTotalCellCount(); CellId++)
	{
		if (GridLayout.GetCellExists(CellId) &&
			GridLayout.GetCellIsAnchor(CellId) &&
			!CellState.DestroyedCells.Contains(CellId) &&
			HasAliveSubCell(CellId, CellState))
		{
			Queue.Enqueue(CellId);
			Connected.Add(CellId);
		}
	}

	// 2. BFS: 通过子单元格边界连通性找到所有可达的单元格
	while (!Queue.IsEmpty())
	{
		int32 CurrCellId;
		Queue.Dequeue(CurrCellId);

		const FIntVector CurrCoord = GridLayout.IdToCoord(CurrCellId);

		for (int32 Dir = 0; Dir < 6; ++Dir)
		{
			const FIntVector NeighborCoord = CurrCoord + FIntVector(
				DIRECTION_OFFSETS[Dir][0],
				DIRECTION_OFFSETS[Dir][1],
				DIRECTION_OFFSETS[Dir][2]
			);

			if (!GridLayout.IsValidCoord(NeighborCoord))
			{
				continue;
			}

			const int32 NeighborCellId = GridLayout.CoordToId(NeighborCoord);

			if (Connected.Contains(NeighborCellId))
			{
				continue;
			}

			if (!GridLayout.GetCellExists(NeighborCellId))
			{
				continue;
			}

			if (CellState.DestroyedCells.Contains(NeighborCellId))
			{
				continue;
			}

			// 检查子单元格边界连通性
			if (HasConnectedBoundary(CurrCellId, NeighborCellId, Dir, CellState))
			{
				Connected.Add(NeighborCellId);
				Queue.Enqueue(NeighborCellId);
			}
		}
	}

	// 3. 不在Connected中的单元格是断开的
	TSet<int32> Disconnected;
	for (int32 CellId = 0; CellId < GridLayout.GetTotalCellCount(); CellId++)
	{
		if (GridLayout.GetCellExists(CellId) &&
			!CellState.DestroyedCells.Contains(CellId) &&
			!Connected.Contains(CellId))
		{
			Disconnected.Add(CellId);
		}
	}

	return Disconnected;
}

//=============================================================================
// FCellDestructionSystem - Hierarchical BFS (SuperCell optimization)
//=============================================================================

namespace HierarchicalBFSHelper
{
	/**
	 * Compute the cell coordinate range of a SuperCell.
	 * Intended for direct coordinate iteration without TArray allocations.
	 */
	struct FSupercellCellRange
	{
		int32 StartX, StartY, StartZ;
		int32 EndX, EndY, EndZ;

		FSupercellCellRange(int32 SupercellId, const FSuperCellState& SupercellState, const FGridCellLayout& GridLayout)
		{
			const FIntVector SupercellCoord = SupercellState.SupercellIdToCoord(SupercellId);
			StartX = SupercellCoord.X * SupercellState.SupercellSize.X;
			StartY = SupercellCoord.Y * SupercellState.SupercellSize.Y;
			StartZ = SupercellCoord.Z * SupercellState.SupercellSize.Z;

			EndX = FMath::Min(StartX + SupercellState.SupercellSize.X, GridLayout.GridSize.X);
			EndY = FMath::Min(StartY + SupercellState.SupercellSize.Y, GridLayout.GridSize.Y);
			EndZ = FMath::Min(StartZ + SupercellState.SupercellSize.Z, GridLayout.GridSize.Z);
		}
	};

	/**
	 * Mark all valid cells in a SuperCell as connected.
	 * Direct coordinate iteration without TArray allocations.
	 */
	void MarkAllCellsInSupercell(
		int32 SupercellId,
		const FSuperCellState& SupercellState,
		const FGridCellLayout& GridLayout,
		const FCellState& CellState,
		TSet<int32>& ConnectedCells)
	{
		const FSupercellCellRange Range(SupercellId, SupercellState, GridLayout);

		for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
		{
			for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
			{
				for (int32 X = Range.StartX; X < Range.EndX; ++X)
				{
					const int32 CellId = GridLayout.CoordToId(X, Y, Z);
					// 排除已销毁的单元格
					if (GridLayout.GetCellExists(CellId) && !CellState.DestroyedCells.Contains(CellId))
					{
						ConnectedCells.Add(CellId);
					}
				}
			}
		}
	}

	/**
	 * Helper to add a neighbor cell (includes subcell-mode branching).
	 */
	FORCEINLINE void TryAddNeighborCell(
		int32 BoundaryCellId,
		int32 NeighborCellId,
		int32 Dir,
		const FGridCellLayout& GridLayout,
		const FCellState& CellState,
		bool bEnableSubcell,
		TQueue<FCellNode>& Queue,
		TSet<int32>& ConnectedCells)
	{
		if (ConnectedCells.Contains(NeighborCellId))
		{
			return;
		}

		if (!GridLayout.GetCellExists(NeighborCellId))
		{
			return;
		}

		if (CellState.DestroyedCells.Contains(NeighborCellId))
		{
			return;
		}

		if (bEnableSubcell)
		{
			if (SubCellBFSHelper::HasConnectedBoundary(BoundaryCellId, NeighborCellId, Dir, CellState))
			{
				ConnectedCells.Add(NeighborCellId);
				Queue.Enqueue(FCellNode::MakeCell(NeighborCellId));
			}
		}
		else
		{
			ConnectedCells.Add(NeighborCellId);
			Queue.Enqueue(FCellNode::MakeCell(NeighborCellId));
		}
	}

	/**
	 * Process a SuperCell node (search adjacent nodes from an intact SuperCell).
	 *
	 * Performance optimizations:
	 * - Use only IsSupercellIntact() (bitfield O(1))
	 * - Direct coordinate iteration without TArray allocations
	 */
	void ProcessSupercellNode(
		int32 SupercellId,
		const FGridCellLayout& GridLayout,
		FSuperCellState& SupercellState,
		const FCellState& CellState,
		bool bEnableSubcell,
		TQueue<FCellNode>& Queue,
		TSet<int32>& ConnectedCells,
		TSet<int32>& VisitedSupercells)
	{
		const FSupercellCellRange Range(SupercellId, SupercellState, GridLayout);
		const FIntVector SupercellCoord = SupercellState.SupercellIdToCoord(SupercellId);

		// 搜索6个方向的相邻SuperCell
		for (int32 Dir = 0; Dir < 6; ++Dir)
		{
			const FIntVector NeighborSCCoord = SupercellCoord + FIntVector(
				DIRECTION_OFFSETS[Dir][0],
				DIRECTION_OFFSETS[Dir][1],
				DIRECTION_OFFSETS[Dir][2]
			);

			if (!SupercellState.IsValidSupercellCoord(NeighborSCCoord))
			{
				continue;
			}

			const int32 NeighborSupercellId = SupercellState.SupercellCoordToId(NeighborSCCoord);

			// 跳过已访问的SuperCell
			if (VisitedSupercells.Contains(NeighborSupercellId))
			{
				continue;
			}

			// 检查相邻的SuperCell是否完好（仅使用位域 - O(1)）
			if (SupercellState.IsSupercellIntact(NeighborSupercellId))
			{
				// 完好的SuperCell -> 添加为SuperCell节点
				VisitedSupercells.Add(NeighborSupercellId);
				Queue.Enqueue(FCellNode::MakeSupercell(NeighborSupercellId));
				MarkAllCellsInSupercell(NeighborSupercellId, SupercellState, GridLayout, CellState, ConnectedCells); 
			}
			else
			{
				// 破碎的SuperCell -> 直接从边界单元连接到邻居单元
				// 直接坐标迭代，无需TArray分配

				// 根据方向遍历边界面的单元格并处理相邻单元格
				switch (Dir)
				{
				case 0: // -X: our X=StartX face -> neighbor cell is X=StartX-1
					for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
					{
						for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
						{
							const int32 BoundaryCellId = GridLayout.CoordToId(Range.StartX, Y, Z);
							const int32 NeighborCellId = GridLayout.CoordToId(Range.StartX - 1, Y, Z);
							if (GridLayout.IsValidCoord(Range.StartX - 1, Y, Z))
							{
								TryAddNeighborCell(BoundaryCellId, NeighborCellId, Dir, GridLayout, CellState, bEnableSubcell, Queue, ConnectedCells);
							}
						}
					}
					break;

				case 1: // +X: our X=EndX-1 face -> neighbor cell is X=EndX
					for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
					{
						for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
						{
							const int32 BoundaryCellId = GridLayout.CoordToId(Range.EndX - 1, Y, Z);
							const int32 NeighborCellId = GridLayout.CoordToId(Range.EndX, Y, Z);
							if (GridLayout.IsValidCoord(Range.EndX, Y, Z))
							{
								TryAddNeighborCell(BoundaryCellId, NeighborCellId, Dir, GridLayout, CellState, bEnableSubcell, Queue, ConnectedCells);
							}
						}
					}
					break;

				case 2: // -Y: our Y=StartY face -> neighbor cell is Y=StartY-1
					for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
					{
						for (int32 X = Range.StartX; X < Range.EndX; ++X)
						{
							const int32 BoundaryCellId = GridLayout.CoordToId(X, Range.StartY, Z);
							const int32 NeighborCellId = GridLayout.CoordToId(X, Range.StartY - 1, Z);
							if (GridLayout.IsValidCoord(X, Range.StartY - 1, Z))
							{
								TryAddNeighborCell(BoundaryCellId, NeighborCellId, Dir, GridLayout, CellState, bEnableSubcell, Queue, ConnectedCells);
							}
						}
					}
					break;

				case 3: // +Y: our Y=EndY-1 face -> neighbor cell is Y=EndY
					for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
					{
						for (int32 X = Range.StartX; X < Range.EndX; ++X)
						{
							const int32 BoundaryCellId = GridLayout.CoordToId(X, Range.EndY - 1, Z);
							const int32 NeighborCellId = GridLayout.CoordToId(X, Range.EndY, Z);
							if (GridLayout.IsValidCoord(X, Range.EndY, Z))
							{
								TryAddNeighborCell(BoundaryCellId, NeighborCellId, Dir, GridLayout, CellState, bEnableSubcell, Queue, ConnectedCells);
							}
						}
					}
					break;

				case 4: // -Z: our Z=StartZ face -> neighbor cell is Z=StartZ-1
					for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
					{
						for (int32 X = Range.StartX; X < Range.EndX; ++X)
						{
							const int32 BoundaryCellId = GridLayout.CoordToId(X, Y, Range.StartZ);
							const int32 NeighborCellId = GridLayout.CoordToId(X, Y, Range.StartZ - 1);
							if (GridLayout.IsValidCoord(X, Y, Range.StartZ - 1))
							{
								TryAddNeighborCell(BoundaryCellId, NeighborCellId, Dir, GridLayout, CellState, bEnableSubcell, Queue, ConnectedCells);
							}
						}
					}
					break;

				case 5: // +Z: our Z=EndZ-1 face -> neighbor cell is Z=EndZ
					for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
					{
						for (int32 X = Range.StartX; X < Range.EndX; ++X)
						{
							const int32 BoundaryCellId = GridLayout.CoordToId(X, Y, Range.EndZ - 1);
							const int32 NeighborCellId = GridLayout.CoordToId(X, Y, Range.EndZ);
							if (GridLayout.IsValidCoord(X, Y, Range.EndZ))
							{
								TryAddNeighborCell(BoundaryCellId, NeighborCellId, Dir, GridLayout, CellState, bEnableSubcell, Queue, ConnectedCells);
							}
						}
					}
					break;
				}
			}
		}

		// 连接到孤儿单元格（从SuperCell边界单元格到外部孤儿单元格）
		// 直接迭代6个边界面，无需TArray分配

		// -X 边界
		for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
		{
			for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
			{
				const int32 BoundaryCellId = GridLayout.CoordToId(Range.StartX, Y, Z);
				const int32 NeighborX = Range.StartX - 1;
				if (GridLayout.IsValidCoord(NeighborX, Y, Z))
				{
					const int32 NeighborCellId = GridLayout.CoordToId(NeighborX, Y, Z);
					if (SupercellState.IsCellOrphan(NeighborCellId))
					{
						TryAddNeighborCell(BoundaryCellId, NeighborCellId, 0, GridLayout, CellState, bEnableSubcell, Queue, ConnectedCells);
					}
				}
			}
		}

		// +X 边界
		for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
		{
			for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
			{
				const int32 BoundaryCellId = GridLayout.CoordToId(Range.EndX - 1, Y, Z);
				const int32 NeighborX = Range.EndX;
				if (GridLayout.IsValidCoord(NeighborX, Y, Z))
				{
					const int32 NeighborCellId = GridLayout.CoordToId(NeighborX, Y, Z);
					if (SupercellState.IsCellOrphan(NeighborCellId))
					{
						TryAddNeighborCell(BoundaryCellId, NeighborCellId, 1, GridLayout, CellState, bEnableSubcell, Queue, ConnectedCells);
					}
				}
			}
		}

		// -Y 边界
		for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
		{
			for (int32 X = Range.StartX; X < Range.EndX; ++X)
			{
				const int32 BoundaryCellId = GridLayout.CoordToId(X, Range.StartY, Z);
				const int32 NeighborY = Range.StartY - 1;
				if (GridLayout.IsValidCoord(X, NeighborY, Z))
				{
					const int32 NeighborCellId = GridLayout.CoordToId(X, NeighborY, Z);
					if (SupercellState.IsCellOrphan(NeighborCellId))
					{
						TryAddNeighborCell(BoundaryCellId, NeighborCellId, 2, GridLayout, CellState, bEnableSubcell, Queue, ConnectedCells);
					}
				}
			}
		}

		// +Y 边界
		for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
		{
			for (int32 X = Range.StartX; X < Range.EndX; ++X)
			{
				const int32 BoundaryCellId = GridLayout.CoordToId(X, Range.EndY - 1, Z);
				const int32 NeighborY = Range.EndY;
				if (GridLayout.IsValidCoord(X, NeighborY, Z))
				{
					const int32 NeighborCellId = GridLayout.CoordToId(X, NeighborY, Z);
					if (SupercellState.IsCellOrphan(NeighborCellId))
					{
						TryAddNeighborCell(BoundaryCellId, NeighborCellId, 3, GridLayout, CellState, bEnableSubcell, Queue, ConnectedCells);
					}
				}
			}
		}

		// -Z 边界
		for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
		{
			for (int32 X = Range.StartX; X < Range.EndX; ++X)
			{
				const int32 BoundaryCellId = GridLayout.CoordToId(X, Y, Range.StartZ);
				const int32 NeighborZ = Range.StartZ - 1;
				if (GridLayout.IsValidCoord(X, Y, NeighborZ))
				{
					const int32 NeighborCellId = GridLayout.CoordToId(X, Y, NeighborZ);
					if (SupercellState.IsCellOrphan(NeighborCellId))
					{
						TryAddNeighborCell(BoundaryCellId, NeighborCellId, 4, GridLayout, CellState, bEnableSubcell, Queue, ConnectedCells);
					}
				}
			}
		}

		// +Z 边界
		for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
		{
			for (int32 X = Range.StartX; X < Range.EndX; ++X)
			{
				const int32 BoundaryCellId = GridLayout.CoordToId(X, Y, Range.EndZ - 1);
				const int32 NeighborZ = Range.EndZ;
				if (GridLayout.IsValidCoord(X, Y, NeighborZ))
				{
					const int32 NeighborCellId = GridLayout.CoordToId(X, Y, NeighborZ);
					if (SupercellState.IsCellOrphan(NeighborCellId))
					{
						TryAddNeighborCell(BoundaryCellId, NeighborCellId, 5, GridLayout, CellState, bEnableSubcell, Queue, ConnectedCells);
					}
				}
			}
		}
	}

	/**
	 * Process a cell node (search adjacent nodes from an individual cell).
	 *
	 * Performance optimization: use only IsSupercellIntact() (bitfield O(1))
	 */
	void ProcessCellNode(
		int32 CellId,
		const FGridCellLayout& GridLayout,
		FSuperCellState& SupercellState,
		const FCellState& CellState,
		bool bEnableSubcell,
		TQueue<FCellNode>& Queue,
		TSet<int32>& ConnectedCells,
		TSet<int32>& VisitedSupercells)
	{
		
		const FIntVector CellCoord = GridLayout.IdToCoord(CellId);

		for (int32 Dir = 0; Dir < 6; ++Dir)
		{
			const FIntVector NeighborCoord = CellCoord + FIntVector(
				DIRECTION_OFFSETS[Dir][0],
				DIRECTION_OFFSETS[Dir][1],
				DIRECTION_OFFSETS[Dir][2]
			);

			if (!GridLayout.IsValidCoord(NeighborCoord))
			{
				continue;
			}

			const int32 NeighborCellId = GridLayout.CoordToId(NeighborCoord);

			if (!GridLayout.GetCellExists(NeighborCellId))
			{
				continue;
			}

			if (CellState.DestroyedCells.Contains(NeighborCellId))
			{
				continue;
			}

			if (ConnectedCells.Contains(NeighborCellId))
			{
				continue;
			}

			// In subcell mode, check boundary connectivity
			bool bIsConnected = true;
			if (bEnableSubcell)
			{
				bIsConnected = SubCellBFSHelper::HasConnectedBoundary(CellId, NeighborCellId, Dir, CellState);
			}

			if (!bIsConnected)
			{
				continue;
			}

			const int32 NeighborSupercellId = SupercellState.GetSupercellForCell(NeighborCellId);

			// 检查邻居是否属于一个完好的SuperCell（仅位域 - O(1)）
			if (NeighborSupercellId != INDEX_NONE &&
			    !VisitedSupercells.Contains(NeighborSupercellId) &&
			    SupercellState.IsSupercellIntact(NeighborSupercellId))
			{
				// 完好的SuperCell -> 扩展到SuperCell节点
				VisitedSupercells.Add(NeighborSupercellId);
				Queue.Enqueue(FCellNode::MakeSupercell(NeighborSupercellId));
				MarkAllCellsInSupercell(NeighborSupercellId, SupercellState, GridLayout, CellState, ConnectedCells);
			}
			else
			{
				// 破碎的SuperCell或孤儿 -> 在单元格级别添加
				ConnectedCells.Add(NeighborCellId);
				Queue.Enqueue(FCellNode::MakeCell(NeighborCellId));
			}
		}
	}

	FORCEINLINE void TryAddNeighborCell_Opt(
		int32 BoundaryCellId,
		int32 NeighborCellId,
		int32 Dir,
		const FGridCellLayout& GridLayout,
		const FCellState& CellState,
		bool bEnableSubcell,
		FConnectivityContext& Context,
		TArray<FCellNode>& Stack)
	{
		if (!GridLayout.GetCellExists(NeighborCellId))
		{
			return;
		}

		if (CellState.DestroyedCells.Contains(NeighborCellId))
		{
			return;
		}
		
		if (Context.IsCellConnected(NeighborCellId))
		{
			return;
		}

		if (bEnableSubcell &&
			!SubCellBFSHelper::HasConnectedBoundary(BoundaryCellId, NeighborCellId, Dir, CellState))
		{
			return;
		}

		Context.SetCellConnected(NeighborCellId);
		Stack.Push(FCellNode::MakeCell(NeighborCellId));
	}

	void MarkAllCellsInSuperCell_Bit(
		int32 SupercellId,
		const FSuperCellState& SupercellState,
		const FGridCellLayout& GridLayout,
		const FCellState& CellState,
		FConnectivityContext& Context		
	)
	{
		const FSupercellCellRange Range(SupercellId, SupercellState, GridLayout);

		for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
		{
			for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
			{
				for (int32 X = Range.StartX; X < Range.EndX; ++X)
				{
					const int32 CellId = GridLayout.CoordToId(X, Y, Z);
					if (GridLayout.GetCellExists(CellId) && !CellState.DestroyedCells.Contains(CellId))
					{
						Context.SetCellConnected(CellId);
					}
				}
			}
		}
	}

	void ProcessSupercellNode_Opt(
		int32 SupercellId,
		const FGridCellLayout& GridLayout,
		FSuperCellState& SupercellState,
		const FCellState& CellState,
		bool bEnableSubcell,
		FConnectivityContext& Context,
		TArray<FCellNode>& Stack
		)
	{
		const FSupercellCellRange Range(SupercellId, SupercellState, GridLayout);
		const FIntVector SupercellCoord = SupercellState.SupercellIdToCoord(SupercellId);

		for (int32 Dir = 0; Dir < 6; Dir++)
		{
			const FIntVector NeighborSCCoord = SupercellCoord + FIntVector(
				DIRECTION_OFFSETS[Dir][0],
				DIRECTION_OFFSETS[Dir][1],
				DIRECTION_OFFSETS[Dir][2]);

			if (!SupercellState.IsValidSupercellCoord(NeighborSCCoord))
			{
				continue;
			}

			const int32 NeighborSupercellId = SupercellState.SupercellCoordToId(NeighborSCCoord);

			if (SupercellState.IsSupercellIntact(NeighborSupercellId))
			{
				if (Context.IsSuperCellVisited(NeighborSupercellId))
				{
					continue;
				}

				Context.SetSuperCellVisited(NeighborSupercellId);
				Stack.Push(FCellNode::MakeSupercell(NeighborSupercellId));
				MarkAllCellsInSuperCell_Bit(NeighborSupercellId, SupercellState, GridLayout, CellState, Context);
				continue;
			}

			switch (Dir)
			{
			case 0: // -X
				for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
				{
					for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
					{
						const int32 BoundaryCellId = GridLayout.CoordToId(Range.StartX, Y, Z);
						if (GridLayout.IsValidCoord(Range.StartX - 1, Y, Z))
						{
							const int32 NeighborCellId = GridLayout.CoordToId(Range.StartX - 1, Y, Z);
							TryAddNeighborCell_Opt(BoundaryCellId, NeighborCellId, Dir, GridLayout, CellState,
							                       bEnableSubcell, Context, Stack);
						}
					}
				}
				break;

			case 1: // +X
				for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
				{
					for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
					{
						const int32 BoundaryCellId = GridLayout.CoordToId(Range.EndX - 1, Y, Z);
						if (GridLayout.IsValidCoord(Range.EndX, Y, Z))
						{
							const int32 NeighborCellId = GridLayout.CoordToId(Range.EndX, Y, Z);
							TryAddNeighborCell_Opt(BoundaryCellId, NeighborCellId, Dir, GridLayout, CellState,
							                       bEnableSubcell, Context, Stack);
						}
					}
				}
				break;

			case 2: // -Y
				for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
				{
					for (int32 X = Range.StartX; X < Range.EndX; ++X)
					{
						const int32 BoundaryCellId = GridLayout.CoordToId(X, Range.StartY, Z);
						if (GridLayout.IsValidCoord(X, Range.StartY - 1, Z))
						{
							const int32 NeighborCellId = GridLayout.CoordToId(X, Range.StartY - 1, Z);
							TryAddNeighborCell_Opt(BoundaryCellId, NeighborCellId, Dir, GridLayout, CellState,
							                       bEnableSubcell, Context, Stack);
						}
					}
				}
				break;

			case 3: // +Y
				for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
				{
					for (int32 X = Range.StartX; X < Range.EndX; ++X)
					{
						const int32 BoundaryCellId = GridLayout.CoordToId(X, Range.EndY - 1, Z);
						if (GridLayout.IsValidCoord(X, Range.EndY, Z))
						{
							const int32 NeighborCellId = GridLayout.CoordToId(X, Range.EndY, Z);
							TryAddNeighborCell_Opt(BoundaryCellId, NeighborCellId, Dir, GridLayout, CellState,
							                       bEnableSubcell, Context, Stack);
						}
					}
				}
				break;

			case 4: // -Z
				for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
				{
					for (int32 X = Range.StartX; X < Range.EndX; ++X)
					{
						const int32 BoundaryCellId = GridLayout.CoordToId(X, Y, Range.StartZ);
						if (GridLayout.IsValidCoord(X, Y, Range.StartZ - 1))
						{
							const int32 NeighborCellId = GridLayout.CoordToId(X, Y, Range.StartZ - 1);
							TryAddNeighborCell_Opt(BoundaryCellId, NeighborCellId, Dir, GridLayout, CellState,
							                       bEnableSubcell, Context, Stack);
						}
					}
				}
				break;

			case 5: // +Z
				for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
				{
					for (int32 X = Range.StartX; X < Range.EndX; ++X)
					{
						const int32 BoundaryCellId = GridLayout.CoordToId(X, Y, Range.EndZ - 1);
						if (GridLayout.IsValidCoord(X, Y, Range.EndZ))
						{
							const int32 NeighborCellId = GridLayout.CoordToId(X, Y, Range.EndZ);
							TryAddNeighborCell_Opt(BoundaryCellId, NeighborCellId, Dir, GridLayout, CellState,
							                       bEnableSubcell, Context, Stack);
						}
					}
				}
				break;
			}
		}

		auto TryAddOrphan = [&](int32 BoundaryCellId, int32 NeighborCellId, int32 OrphanDir)
		{
			if (SupercellState.IsCellOrphan(NeighborCellId))
			{
				TryAddNeighborCell_Opt(BoundaryCellId, NeighborCellId, OrphanDir, GridLayout, CellState,
				                       bEnableSubcell, Context, Stack);
			}
		};

		// -X 边界
		for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
		{
			for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
			{
				const int32 BoundaryCellId = GridLayout.CoordToId(Range.StartX, Y, Z);
				if (GridLayout.IsValidCoord(Range.StartX - 1, Y, Z))
				{
					TryAddOrphan(BoundaryCellId, GridLayout.CoordToId(Range.StartX - 1, Y, Z), 0);
				}
			}
		}

		// +X
		for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
		{
			for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
			{
				const int32 BoundaryCellId = GridLayout.CoordToId(Range.EndX - 1, Y, Z);
				if (GridLayout.IsValidCoord(Range.EndX, Y, Z))
				{
					TryAddOrphan(BoundaryCellId, GridLayout.CoordToId(Range.EndX, Y, Z), 1);
				}
			}
		}

		// -Y
		for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
		{
			for (int32 X = Range.StartX; X < Range.EndX; ++X)
			{
				const int32 BoundaryCellId = GridLayout.CoordToId(X, Range.StartY, Z);
				if (GridLayout.IsValidCoord(X, Range.StartY - 1, Z))
				{
					TryAddOrphan(BoundaryCellId, GridLayout.CoordToId(X, Range.StartY - 1, Z), 2);
				}
			}
		}

		// +Y
		for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
		{
			for (int32 X = Range.StartX; X < Range.EndX; ++X)
			{
				const int32 BoundaryCellId = GridLayout.CoordToId(X, Range.EndY - 1, Z);
				if (GridLayout.IsValidCoord(X, Range.EndY, Z))
				{
					TryAddOrphan(BoundaryCellId, GridLayout.CoordToId(X, Range.EndY, Z), 3);
				}
			}
		}

		// -Z
		for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
		{
			for (int32 X = Range.StartX; X < Range.EndX; ++X)
			{
				const int32 BoundaryCellId = GridLayout.CoordToId(X, Y, Range.StartZ);
				if (GridLayout.IsValidCoord(X, Y, Range.StartZ - 1))
				{
					TryAddOrphan(BoundaryCellId, GridLayout.CoordToId(X, Y, Range.StartZ - 1), 4);
				}
			}
		}

		// +Z
		for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
		{
			for (int32 X = Range.StartX; X < Range.EndX; ++X)
			{
				const int32 BoundaryCellId = GridLayout.CoordToId(X, Y, Range.EndZ - 1);
				if (GridLayout.IsValidCoord(X, Y, Range.EndZ))
				{
					TryAddOrphan(BoundaryCellId, GridLayout.CoordToId(X, Y, Range.EndZ), 5);
				}
			}
		}		
	}
}   

TSet<int32> FCellDestructionSystem::FindDisconnectedCellsHierarchicalLevel(
	const FGridCellLayout& GridLayout,
	FSuperCellState& SupercellState,
	const FCellState& CellState,
	bool bEnableSubcell,
	FConnectivityContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CellStructure_FindDisconnectedCellsHierarchicalLevel);

	// 1. 查找连接到锚点的单元格
	FindConnectedCellsHierarchical_Optimized(
		GridLayout, SupercellState, CellState, Context, bEnableSubcell);
	
	// if (!DEBUGStruct::BFSTest())
	// {
	// 	ConnectedCells = FindConnectedCellsHierarchical(
	// 	   GridLayout, SupercellState, CellState, bEnableSubcell);
	// }
	// else
	// {
	// 	ConnectedCells = FindConnectedCellsHierarchical_Optimized(
	// 	GridLayout, SupercellState, CellState, Context, bEnableSubcell);
	// }

	// 2. Cells not in Connected are disconnected
	TSet<int32> Disconnected;

	for (int32 CellId = 0; CellId < GridLayout.GetTotalCellCount(); ++CellId)
	{
		if (!GridLayout.GetCellExists(CellId))
		{
			continue;
		}

		if (CellState.DestroyedCells.Contains(CellId))
		{
			continue;
		}

		if (!Context.IsCellConnected(CellId))
		{
			Disconnected.Add(CellId);
		}
	}
	
	return Disconnected;
}

void FCellDestructionSystem::FindConnectedCellsHierarchical_Optimized(
	const FGridCellLayout& Cache,
	FSuperCellState& SupercellState,
	const FCellState& CellState,
	FConnectivityContext& Context,
	bool bEnableSubcell)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CellStructure_FindConnectedCellsHierarchical_Opt);
	
	using namespace HierarchicalBFSHelper;
	int32 TotalCells = Cache.GetTotalCellCount();
	
	Context.Reset(TotalCells, SupercellState.GetTotalSupercellCount());

	TArray<FCellNode>& Stack = Context.WorkStack;

	const int32 SizeX = Cache.GridSize.X;
	const int32 SizeY = Cache.GridSize.Y;
	const int32 SizeXY = Cache.GridSize.X * Cache.GridSize.Y;

	const int32 Strides[6] = {-1, 1, -SizeX, SizeX, -SizeXY, SizeXY};

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CellStructure_FindConnectedCellsHierarchical_Opt_Anchor);
		for (int32 CellId = 0; CellId < TotalCells; CellId++)
		{
			if (!Cache.GetCellExists(CellId))
			{
				continue;
			}
		
			if (!Cache.GetCellIsAnchor(CellId))
			{
				continue;
			}		

			if (CellState.DestroyedCells.Contains(CellId))
			{
				continue;
			}

			if (bEnableSubcell && !SubCellBFSHelper::HasAliveSubCell(CellId, CellState))
			{
				continue;
			}

			const int32 SupercellId = SupercellState.GetSupercellForCell(CellId);

			if (SupercellId != INDEX_NONE &&
				SupercellState.IsSupercellIntact(SupercellId) &&
				!Context.IsSuperCellVisited(SupercellId))
			{
				Context.SetSuperCellVisited(SupercellId);
				Stack.Push(FCellNode::MakeSupercell(SupercellId));
				MarkAllCellsInSuperCell_Bit(SupercellId, SupercellState, Cache, CellState, Context);
			}
			else
			{
				if (!Context.IsCellConnected(CellId))
				{
					Context.SetCellConnected(CellId);
					Stack.Push(FCellNode::MakeCell(CellId));
				}
			}
		}
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CellStructure_FindConnectedCellsHierarchical_Opt_DFS);
		while (Stack.Num() > 0)
		{
			const FCellNode Current = Stack.Pop(EAllowShrinking::No);

			if (Current.bIsSupercell)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(CellStructure_FindConnectedCellsHierarchical_Opt_Intact);
				ProcessSupercellNode_Opt(
					Current.Id,
					Cache,
					SupercellState,
					CellState,
					bEnableSubcell,
					Context,
					Stack);
			}
			else
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(CellStructure_FindConnectedCellsHierarchical_Opt_Cell);
				const int32 CurrentId = Current.Id;

				const int32 Z = CurrentId / SizeXY;
				const int32 RemXY = CurrentId - Z * SizeXY;
				const int32 Y = RemXY / SizeX;
				const int32 X = RemXY - Y * SizeX;

				for (int32 Dir = 0; Dir < 6; ++Dir)
				{
					// -X 边界
					if (Dir == 0 && X == 0)
					{
						continue;
					}
					// +X 边界
					if (Dir == 1 && X == SizeX - 1)
					{
						continue;
					}				
				
					// -Y 边界
					if (Dir == 2 && Y == 0)
					{
						continue;
					}
					// +Y 边界
					if (Dir == 3 && Y >= (SizeY - 1))
					{
						continue;
					}

					// -Z 边界
					if (Dir == 4 && Z == 0)
					{
						continue;
					}
					// +Z 边界
					if (Dir == 5 && Z == Cache.GridSize.Z - 1)
					{
						continue;
					}

					const int32 NeighborId = CurrentId + Strides[Dir];

					if (!Cache.GetCellExists(NeighborId))
					{
						continue;
					}

					if (CellState.DestroyedCells.Contains(NeighborId))
					{
						continue;
					}

					if (Context.IsCellConnected(NeighborId))
					{
						continue;
					}
				
					if (bEnableSubcell && !SubCellBFSHelper::HasConnectedBoundary(CurrentId, NeighborId, Dir, CellState))
					{
						continue;
					}

					const int32 NeighborSupercellId = SupercellState.GetSupercellForCell(NeighborId);

					if (NeighborSupercellId != INDEX_NONE &&
						SupercellState.IsSupercellIntact(NeighborSupercellId) &&
						!Context.IsSuperCellVisited(NeighborSupercellId))
					{
						Context.SetSuperCellVisited(NeighborSupercellId);
						Stack.Push(FCellNode::MakeSupercell(NeighborSupercellId));
						MarkAllCellsInSuperCell_Bit(NeighborSupercellId, SupercellState, Cache, CellState, Context);
					}
					else
					{
						Context.SetCellConnected(NeighborId);
						Stack.Push(FCellNode::MakeCell(NeighborId));
					}
				}
			}
		}
	}

	// TSet<int32> ConnectedCells(Context.ConnectedCellIds);
	//
	// return ConnectedCells;
} 

TSet<int32> FCellDestructionSystem::FindDisconnectedCellsFromAffected(
	const FGridCellLayout& Cache,
	FSuperCellState& SupercellState,
	const FCellState& CellState,
	const TArray<int32>& AffectedNeighborCells,
	FConnectivityContext& Context,
	bool bEnableSupercell,
	bool bEnableSubcell)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DFSToAnchor_FindDisconnectedCellsFromAffected);
	using namespace HierarchicalBFSHelper;

	TSet<int32> DisconnectedCells;
	TSet<int32> ConfirmedConnected;

	const int32 TotalCells = Cache.GetTotalCellCount();
	const int32 SizeX = Cache.GridSize.X;
	const int32 SizeY = Cache.GridSize.Y;
	const int32 SizeZ = Cache.GridSize.Z;
	const int32 SizeXY = SizeX * SizeY;

	// CellId = X + Y * SizeX + Z * SizeX * SizeY
	// x轴移动 = 1, y轴移动 = sizeX, z轴移动 = sizeX * sizeY 
	const int32 Stride[6] = { -1, 1, -SizeX, SizeX, -SizeXY, SizeXY };

	for (int32 StartCellId : AffectedNeighborCells)
	{
		// 已经确定过的 Cell Id 
		if (ConfirmedConnected.Contains(StartCellId) || DisconnectedCells.Contains(StartCellId))
		{
			continue;
		}

		// Skip Destroyed cells
		if (CellState.DestroyedCells.Contains(StartCellId))
		{
			continue;
		}

		// Skip non-existent cells
		if (!Cache.GetCellExists(StartCellId))
		{
			continue;	
		}

		// Reset context for this search
		Context.Reset(TotalCells, SupercellState.GetTotalSupercellCount());
		 
		TArray<FCellNode>& Stack = Context.WorkStack;
		bool bFoundAnchor = false;

		if (bEnableSupercell)
		{
			const int32 SupercellId = SupercellState.GetSupercellForCell(StartCellId);

			// Supercell存在 && 未损坏 
			if (SupercellId != INDEX_NONE && SupercellState.IsSupercellIntact(SupercellId))
			{	
				// 到达了包含锚点的Supercell
				if (SupercellContainsAnchor(SupercellId, Cache, SupercellState, CellState))
				{
					bFoundAnchor = true;
				}

				// 包含了已经确认连接的
				else if (SupercellContainsConfirmedConnected(SupercellId, Cache, SupercellState, ConfirmedConnected))
				{
					bFoundAnchor = true;
				}
				else
				{
					Context.SetSuperCellVisited(SupercellId);
					Stack.Push(FCellNode::MakeSupercell(SupercellId));
					MarkAllCellsInSuperCell_Bit(SupercellId, SupercellState, Cache, CellState, Context); // 已经是intact false了，一次性打上bit不就好了吗？

				}
			}
			else
			{
				// Broken SuperCell 或 Orphan → Push为Cell
				if (Cache.GetCellIsAnchor(StartCellId))
				{
					bFoundAnchor = true;
				}
				else if (ConfirmedConnected.Contains(StartCellId))
				{
					bFoundAnchor = true;
				}
				else
				{
					Context.SetCellConnected(StartCellId);
					Stack.Push(FCellNode::MakeCell(StartCellId));
				}
			}
		}
		// 不使用Supercell的情况
		else
		{
			if (Cache.GetCellIsAnchor(StartCellId))
			{
				bFoundAnchor = true;
			}
			else if (ConfirmedConnected.Contains(StartCellId))
			{
				bFoundAnchor = true;
			}
			else
			{
				Context.SetCellConnected(StartCellId);
				Stack.Push(FCellNode::MakeCell(StartCellId));
			}
		}

		// DFS Loop
		while (!bFoundAnchor && Stack.Num() > 0)
		{
			// EAllowShrinking：删除元素时不减小内存（容量）
			const FCellNode Current = Stack.Pop(EAllowShrinking::No);

			if (Current.bIsSupercell)
			{
				const int32 SupercellId = Current.Id;
				const FSupercellCellRange Range(SupercellId, SupercellState, Cache);
				const FIntVector SupercellCoord = SupercellState.SupercellIdToCoord(SupercellId);

				for (int32 Dir = 0; Dir < 6 && !bFoundAnchor; ++Dir)
				{
					const FIntVector NeighborsSCCoord = SupercellCoord + FIntVector(
					DIRECTION_OFFSETS[Dir][0] ,
					DIRECTION_OFFSETS[Dir][1] ,
					DIRECTION_OFFSETS[Dir][2]
					);

					if (!SupercellState.IsValidSupercellCoord(NeighborsSCCoord))
					{
						continue;
					}

					const int32 NeighborSupercellId = SupercellState.SupercellCoordToId(NeighborsSCCoord);

					// 当Supercell未损坏时
					if (SupercellState.IsSupercellIntact(NeighborSupercellId))
					{
						// 如果已经确认过，则跳过
						if (Context.IsSuperCellVisited(NeighborSupercellId))
						{
							continue;
						}

						// 是包含锚点的Supercell吗
						if (SupercellContainsAnchor(NeighborSupercellId,  Cache, SupercellState, CellState))
						{
							bFoundAnchor = true;
							break;
						}

						// 包含已确认连接的Supercell
						if (SupercellContainsConfirmedConnected(NeighborSupercellId, Cache, SupercellState, ConfirmedConnected))
						{
							bFoundAnchor = true;
							break;
						}

						Context.SetSuperCellVisited(NeighborSupercellId);
						Stack.Push(FCellNode::MakeSupercell(NeighborSupercellId));
						MarkAllCellsInSuperCell_Bit(NeighborSupercellId, SupercellState, Cache, CellState, Context);
					}
					else
					{
						// 已经破碎的Supercell  
						TArray<int32> BoundaryCellIds;
						SupercellState.GetBoundaryCellsInDirection(SupercellId, Dir, Cache, BoundaryCellIds);

						for (int32 BoundaryCellId : BoundaryCellIds)
						{
							// 跳过不存在或已摧毁的cell
							if (!Cache.GetCellExists(BoundaryCellId) || CellState.DestroyedCells.Contains(BoundaryCellId))
							{
								continue;
							}

							const FIntVector BoundaryCoord = Cache.IdToCoord(BoundaryCellId);
							const FIntVector NeighborCoord = BoundaryCoord + FIntVector(
								DIRECTION_OFFSETS[Dir][0],
								DIRECTION_OFFSETS[Dir][1],
								DIRECTION_OFFSETS[Dir][2]
							);
						
							if (!Cache.IsValidCoord(NeighborCoord))
							{
								continue;
							}

							const int32 NeighborCellId = Cache.CoordToId(NeighborCoord);

							// 跳过已经访问过或被摧毁的Cell
							if (!Cache.GetCellExists(NeighborCellId) || CellState.DestroyedCells.Contains(NeighborCellId) || Context.IsCellConnected(NeighborCellId))
							{
								continue;
							}

							if (bEnableSubcell && !SubCellBFSHelper::HasConnectedBoundary(BoundaryCellId, NeighborCellId, Dir, CellState))
							{
								continue;
							}

							if (Cache.GetCellIsAnchor(NeighborCellId))
							{
								bFoundAnchor = true;
								break;
							}

							if (ConfirmedConnected.Contains(NeighborCellId))
							{
								bFoundAnchor = true;
								break;
							}

							Context.SetCellConnected(NeighborCellId);
							Stack.Push(FCellNode::MakeCell(NeighborCellId));
						}

					}

				}

			}
			else
			{
				// 处理Cell节点
				const int32 CurrentCellId = Current.Id;

				//CellId = X + Y * SizeX + Z * SizeX * SizeY
				const int32 Z = CurrentCellId / SizeXY;
				const int32 RemXY = CurrentCellId - Z * SizeXY;
				const int32 Y = RemXY / SizeX;
				const int32 X = RemXY - Y * SizeX;

				// 正向：-X, X, -Y, Y, -Z, Z
				for (int32 Dir = 5 ; Dir >= 0 && !bFoundAnchor; --Dir)
				{
					// 边界检查
					if (Dir == 0 && X == 0)
					{
						continue;
					}
					if (Dir == 1 && X == SizeX - 1)
					{
						continue;
					}
					if (Dir == 2 && Y == 0)
					{
						continue;
					}
					if (Dir == 3 && Y == SizeY - 1)
					{
						continue;
					}
					if (Dir == 4 && Z == 0)
					{
						continue;
					}
					if (Dir == 5 && Z == SizeZ - 1)
					{
						continue;
					}

					const int32 NeighborCellId = CurrentCellId + Stride[Dir];

					// 基本检查
					if (!Cache.GetCellExists(NeighborCellId) ||
						CellState.DestroyedCells.Contains(NeighborCellId) ||
						Context.IsCellConnected(NeighborCellId))
					{
						continue;
					}

					// SubCell边界连接性检查
					if (bEnableSubcell &&
						!SubCellBFSHelper::HasConnectedBoundary(CurrentCellId, NeighborCellId, Dir, CellState))
					{
						continue;
					}

					// 到达锚点检查
					if (Cache.GetCellIsAnchor(NeighborCellId))
					{
						bFoundAnchor = true;
						break;
					}

					// 到达已确认连接的检查
					if (ConfirmedConnected.Contains(NeighborCellId))
					{
						bFoundAnchor = true;
						break;
					}

					// 检查邻居是否属于一个完整的SuperCell
					if (bEnableSupercell)
					{
						const int32 NeighborSupercellId = SupercellState.GetSupercellForCell(NeighborCellId);

						if (NeighborSupercellId != INDEX_NONE &&
							SupercellState.IsSupercellIntact(NeighborSupercellId) &&
							!Context.IsSuperCellVisited(NeighborSupercellId))
						{
							// 完整的SuperCell -> 检查锚点/ConfirmedConnected后作为SuperCell推入
							if (SupercellContainsAnchor(NeighborSupercellId, Cache, SupercellState, CellState))
							{
								bFoundAnchor = true;
								break;
							}

							if (SupercellContainsConfirmedConnected(NeighborSupercellId, Cache, SupercellState, ConfirmedConnected))
							{
								bFoundAnchor = true;
								break;
							}

							Context.SetSuperCellVisited(NeighborSupercellId);
							Stack.Push(FCellNode::MakeSupercell(NeighborSupercellId));
							MarkAllCellsInSuperCell_Bit(NeighborSupercellId, SupercellState, Cache, CellState, Context);
							continue;
						}
					}

					// 破碎的SuperCell或孤儿 ->作为Cell推入
					Context.SetCellConnected(NeighborCellId);
					Stack.Push(FCellNode::MakeCell(NeighborCellId));
				}
			}

		}

		if (bFoundAnchor)
		{
			for (int32 CellId : Context.ConnectedCellIds)
			{
				ConfirmedConnected.Add(CellId);
			}
		}

		else
		{
			for (int32 CellId : Context.ConnectedCellIds)
			{
				DisconnectedCells.Add(CellId);
			}
		}
	}
	 
	return DisconnectedCells;
}
bool FCellDestructionSystem::SupercellContainsAnchor(
	int32 SupercellId,
	const FGridCellLayout& Cache,
	const FSuperCellState& SupercellState,
	const FCellState& CellState)
{
	using namespace HierarchicalBFSHelper;

	const FSupercellCellRange Range(SupercellId, SupercellState, Cache);

	for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
	{
		for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
		{
			for (int32 X = Range.StartX; X < Range.EndX; ++X)
			{
				const int32 CellId = Cache.CoordToId(X, Y, Z);

				if (Cache.GetCellExists(CellId) &&
					!CellState.DestroyedCells.Contains(CellId) &&
					Cache.GetCellIsAnchor(CellId))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool FCellDestructionSystem::SupercellContainsConfirmedConnected(
	int32 SupercellId,
	const FGridCellLayout& Cache,
	const FSuperCellState& SupercellState,
	const TSet<int32>& ConfirmedConnected
)
{
	using namespace HierarchicalBFSHelper;

	const FSupercellCellRange Range(SupercellId, SupercellState, Cache);

	for (int32 Z = Range.StartZ; Z < Range.EndZ; ++Z)
	{
		for (int32 Y = Range.StartY; Y < Range.EndY; ++Y)
		{
			for (int32 X = Range.StartX; X < Range.EndX; ++X)
			{
				const int32 CellId = Cache.CoordToId(X, Y, Z);
				if (ConfirmedConnected.Contains(CellId))
				{
					return true;
				}
			}
		}
	}

	return false;
}