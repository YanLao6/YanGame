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
#include "StructuralIntegrity/GridCellTypes.h"

/**
 * 单元格销毁评估系统。
 * 基于 BFS 的结构完整性检查和销毁评估。
 */
class REALTIMEDESTRUCTION_API FCellDestructionSystem
{
public:
	//=========================================================================
	// 单元格销毁评估
	//=========================================================================

	/**
	 * 根据销毁形状计算被销毁的单元格ID。
	 * 使用中心点+顶点混合测试。
	 *
	 * @param Cache - 网格布局
	 * @param Shape - 销毁形状（量化）
	 * @param MeshTransform - 网格世界变换
	 * @param DestroyedCells - 已销毁的单元格（排除）
	 * @return 新销毁的单元格ID
	 */
	static TArray<int32> ProcessCellDestruction(
		const FGridCellLayout& Cache,
		const FQuantizedDestructionInput& Shape,
		const FTransform& MeshTransform,
		const TSet<int32>& DestroyedCells);

	/**
	 * 根据销毁形状计算被销毁的单元格ID。
	 * 使用中心点+顶点混合测试。
	 *
	 * @param Cache - 网格布局
	 * @param Shape - 销毁形状（量化）
	 * @param MeshTransform - 网格世界变换
	 * @param InOutCellState - 单元格状态（排除DestroyedCells）
	 * @return FDestructionResult，其中填充了NewlyDestroyedCells
	 */
	static FDestructionResult ProcessCellDestruction(
		const FGridCellLayout& Cache,
		const FQuantizedDestructionInput& Shape,
		const FTransform& MeshTransform,
		FCellState& InOutCellState);

	/*
	 * <<<SubCell Level API>>>
	 * 执行子单元级别的销毁。
	 * 单元销毁通过子单元处理，而不是中心点+顶点混合测试。
	 */
	static FDestructionResult ProcessCellDestructionSubCellLevel(
		const FGridCellLayout& Cache,
		const FQuantizedDestructionInput& Shape,
		const FTransform& MeshTransform,
		FCellState& InOutCellState);
	
	/**
	 * 检查单个单元格是否被销毁。
	 * 阶段1：中心点测试（快速）
	 * 阶段2：多数顶点测试（边缘情况）
	 */
	static bool IsCellDestroyed(
		const FGridCellLayout& Cache,
		int32 CellId,
		const FQuantizedDestructionInput& Shape,
		const FTransform& MeshTransform);

	
	//=========================================================================
	// 结构完整性检查 (BFS)
	//=========================================================================

	/**
	 * 查找与锚点分离的单元格（统一API）。
	 *
	 * 根据 bEnableSupercell/bEnableSubcell 选择实现：
	 * - 启用 SuperCell：分层 BFS (SuperCell + Cell/SubCell)
	 * - 仅 SubCell：子单元级 BFS
	 * - 两者都禁用：单元格级 BFS
	 *
	 * @param Cache - 网格布局
	 * @param SupercellState - SuperCell 状态（当 bEnableSupercell=true 时使用）
	 * @param CellState - 单元格状态
	 * @param bEnableSupercell - 是否使用 SuperCell BFS
	 * @param bEnableSubcell - 是否使用子单元连接性检查
	 * @return 分离的单元格ID集合
	 */
	static TSet<int32> FindDisconnectedCells(
		const FGridCellLayout& Cache,
		FSuperCellState& SupercellState,
		const FCellState& CellState,
		bool bEnableSupercell,
		bool bEnableSubcell,
		FConnectivityContext& Context);

	/**
	 * <<<Cell Level API>>>
	 * 查找与锚点分离的单元格。
	 * 推荐通过 FindDisconnectedCells 调用。
	 *
	 * @param Cache - 网格布局
	 * @param DestroyedCells - 已销毁的单元格集合
	 * @return 分离的单元格ID集合
	 */
	static TSet<int32> FindDisconnectedCellsCellLevel(
		const FGridCellLayout& Cache,
		const TSet<int32>& DestroyedCells);

	/**
	 * <<<SubCell Level API>>>
	 * 查找与锚点分离的单元格（子单元级连接性）。
	 * 推荐通过 FindDisconnectedCells 调用。
	 *
	 * 使用子单元级 BFS 测试到锚点的可达性。
	 * 从所有锚点开始，通过子单元边界连接性进行遍历。
	 *
	 * @param Cache - 网格布局
	 * @param CellState - 单元格状态（包括子单元状态）
	 * @return 分离的单元格ID集合
	 */
	static TSet<int32> FindDisconnectedCellsSubCellLevel(
		const FGridCellLayout& Cache,
		const FCellState& CellState);

	/**
	 * 通过分层 BFS 查找分离的单元格。
	 *
	 * 调用 FindConnectedCellsHierarchical() 并返回未连接的单元格。
	 * 推荐通过 FindDisconnectedCells 调用。
	 *
	 * @param Cache - 网格布局
	 * @param SupercellState - SuperCell 状态
	 * @param CellState - 单元格状态
	 * @param bEnableSubcell - 是否启用子单元模式
	 * @return 分离的单元格ID集合
	 */
	static TSet<int32> FindDisconnectedCellsHierarchicalLevel(
		const FGridCellLayout& Cache,
		FSuperCellState& SupercellState,
		const FCellState& CellState,
		bool bEnableSubcell,
		FConnectivityContext& Context);

	static void FindConnectedCellsHierarchical_Optimized(
		const FGridCellLayout& Cache,
		FSuperCellState& SupercellState,
		const FCellState& CellState,
		FConnectivityContext& Context,
		bool bEnableSubcell);

	static TSet<int32> FindDisconnectedCellsFromAffected(
		const FGridCellLayout& Cache,
		FSuperCellState& SupercellState,
		const FCellState& CellState,
		const TArray<int32>& AffectedNeighborCells,
		FConnectivityContext& Context,
		bool bEnableSupercell,
		bool bEnableSubcell );

	static bool SupercellContainsAnchor(
		int32 SupercellId,
		const FGridCellLayout& Cache,
		const FSuperCellState& SupercellState,
		const FCellState& CellState);

	static bool SupercellContainsConfirmedConnected(
		int32 SupercellId,
		const FGridCellLayout& Cache,
		const FSuperCellState& SupercellState,
		const TSet<int32>& ConfirmedConnected
	);

	//=========================================================================
	// 分组分离的单元格
	//=========================================================================
	/**
	 * 将分离的单元格分组为连接的组。
	 *
	 * @param Cache - 网格布局
	 * @param DisconnectedCells - 分离的单元格
	 * @param DestroyedCells - 已销毁的单元格（排除边界）
	 * @return 每组的单元格ID列表
	 */
	static TArray<TArray<int32>> GroupDetachedCells(
		const FGridCellLayout& Cache,
		const TSet<int32>& DisconnectedCells,
		const TSet<int32>& DestroyedCells);
	
	//=========================================================================
	// 实用工具
	//=========================================================================

	/**
	 * 计算单元格组的中心。
	 */
	static FVector CalculateGroupCenter(
		const FGridCellLayout& Cache,
		const TArray<int32>& CellIds,
		const FTransform& MeshTransform);

	/**
	 * 计算初始碎片速度（爆炸方向）。
	 */
	static FVector CalculateDebrisVelocity(
		const FVector& DebrisCenter,
		const TArray<FQuantizedDestructionInput>& DestructionInputs,
		float BaseSpeed = 500.0f);

	/**
	 * 检查单元格是否为边界单元格（与已销毁的单元格相邻）。
	 */
	static bool IsBoundaryCell(
		const FGridCellLayout& Cache,
		int32 CellId,
		const TSet<int32>& DestroyedCells);
};

/**
 * [已弃用] 服务器销毁批处理程序。
 * 收集销毁事件并以 16.6ms 的节奏处理它们。
 *
 * 当前未使用。需要重构以支持 SuperCell/SubCell 统一 API
 * (向 SetContext 添加 SupercellCache 和 bEnableSubcell)。
 */
class UE_DEPRECATED(5.0, "FDestructionBatchProcessor is not currently used. Requires refactoring for SuperCell/SubCell support.")
	REALTIMEDESTRUCTION_API FDestructionBatchProcessor
{
public:
	/** 批处理间隔 (16.6ms = 60fps)。 */
	static constexpr float BatchInterval = 1.0f / 60.0f;

	FDestructionBatchProcessor();

	/**
	 * 将销毁请求排队（不立即处理）。
	 */
	void QueueDestruction(const FCellDestructionShape& Shape);

	/**
	 * Tick 处理（检查批处理间隔）。
	 * @return 如果处理了一个批次，则为 True
	 */
	bool Tick(float DeltaTime);

	/**
	 * 强制处理当前队列（立即处理）。
	 */
	void FlushQueue();

	/**
	 * 获取上一个批处理结果（处理后调用）。
	 */
	const FBatchedDestructionEvent& GetLastBatchResult() const { return LastBatchResult; }

	/**
	 * 检查是否有待处理的销毁。
	 */
	bool HasPendingDestructions() const { return PendingDestructions.Num() > 0; }

	/**
	 * 设置处理上下文（必须在批处理之前调用）。
	 */
	void SetContext(
		const FGridCellLayout* InCache,
		FCellState* InCellState,
		const FTransform& InMeshTransform);

private:
	/** 实际的批处理。 */
	void ProcessBatch();

	/** 在 16.6ms 内累积的销毁请求。 */
	TArray<FQuantizedDestructionInput> PendingDestructions;

	/** 累积计时器。 */
	float AccumulatedTime;

	/** 上一个批处理结果。 */
	FBatchedDestructionEvent LastBatchResult;

	/** 处理上下文。 */
	const FGridCellLayout* LayoutPtr;
	FCellState* CellStatePtr;
	FTransform MeshTransform;

	/** 碎片 ID 计数器。 */
	int32 DebrisIdCounter;
};
