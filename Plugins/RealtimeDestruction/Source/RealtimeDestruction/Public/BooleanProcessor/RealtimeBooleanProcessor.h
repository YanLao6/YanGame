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
#include <atomic>
#include "UObject/WeakObjectPtr.h"
#include "Containers/Queue.h"
#include "DynamicMesh/MeshTangents.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "HAL/CriticalSection.h"

////////////////////////////////////////
/******** 前向声明 ********/
namespace UE::Geometry
{
	class FDynamicMesh3;
}
class UDynamicMeshComponent;
class UPrimitiveComponent;
struct FRealtimeDestructionOp;
struct FGeometryScriptMeshBooleanOptions;
struct FGeometryScriptPlanarSimplifyOptions;
enum class EGeometryScriptBooleanOperation : uint8;
class URealtimeDestructibleMeshComponent;
class UDecalComponent;
class URDMThreadManagerSubsystem;
////////////////////////////////////////

enum class EBooleanWorkType : uint8
{
	BulletHole,
	IslandRemoval
};

// 前向声明
class ADebrisActor;

struct FIslandRemovalContext
{
	std::atomic<int32> RemainingTaskCount{0};
	UE::Geometry::FDynamicMesh3 AccumulatedDebrisMesh;

	FCriticalSection MeshLock;
	TWeakObjectPtr<URealtimeDestructibleMeshComponent> Owner;

	/** 客户端：将网格应用到已有 DebrisActor（为 null 时调用 SpawnDebrisActor） */
	TWeakObjectPtr<ADebrisActor> TargetDebrisActor;

	/** 清理用：所有任务完成后传入 CleanupSmallFragments 的断连 Cell ID 集合 */
	TSet<int32> DisconnectedCellsForCleanup;
};

/** 单个 Chunk 的 Union 结果载荷，包含合并的 Tool Mesh 及 Decal 列表 */
struct FUnionResult
{
	int32 BatchID = 0;				                     // 用于排序
	UE::Geometry::FDynamicMesh3 PendingCombinedToolMesh; // Union 结果网格
	TArray<TWeakObjectPtr<UDecalComponent>> Decals;
	int32 UnionCount = 0;

	// Chunk 级别字段
	int32 ChunkIndex = INDEX_NONE;
	TWeakObjectPtr<UDynamicMeshComponent> TargetChunkMesh = nullptr;

	TSharedPtr<UE::Geometry::FDynamicMesh3> SharedToolMesh = nullptr;
	TSharedPtr<UE::Geometry::FDynamicMesh3> DebrisSharedToolMesh = nullptr;
	TSharedPtr<UE::Geometry::FDynamicMesh3> OutDebrisMesh = nullptr;
	TSharedPtr<FIslandRemovalContext> IslandContext;
	EBooleanWorkType WorkType = EBooleanWorkType::BulletHole;

	/** Batch 完成追踪 ID 数组（多个 Op 可被合并成一次 Union 处理） */
	TArray<int32> CompletionBatchIds;
};

/** 排队等待 Boolean 处理的单次 Tool 碰撞请求 */
struct FBulletHole
{
	// TODO: 考虑切换为 SoA 布局

	FTransform ToolTransform = {};
	uint8 Attempts = 0;
	static constexpr uint8 MaxAttempts = 2;

	// true：穿透型；false：非穿透型
	bool bIsPenetration = false;

	TWeakObjectPtr<UDecalComponent> TemporaryDecal = nullptr;

	TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe> ToolMeshPtr = nullptr;

	TWeakObjectPtr<UDynamicMeshComponent> TargetMesh = nullptr;

	int32 ChunkIndex = INDEX_NONE;

	/** Batch 完成追踪 ID（INDEX_NONE 表示不追踪） */
	int32 BatchId = INDEX_NONE;

	bool CanRetry() const { return Attempts <= MaxAttempts; }

	void Reset()
	{
		ToolTransform = {};
		Attempts = 0;
		bIsPenetration = false;
		TemporaryDecal = nullptr;
		ToolMeshPtr = nullptr;
		TargetMesh = nullptr;
		ChunkIndex = INDEX_NONE;
		BatchId = INDEX_NONE;
	}
};

/** 批量子弹孔数据（SoA 布局），用于对单个 Chunk 执行 Union/Subtract */
struct FBulletHoleBatch
{
	TArray<FTransform> ToolTransforms = {};
	TArray<uint8> Attempts = {};
	TArray<bool> bIsPenetrations = {};
	TArray<TWeakObjectPtr<UDecalComponent>> TemporaryDecals = {};
	TArray<TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe>> ToolMeshPtrs = {};
	TArray<int32> CompletionBatchIds = {};  // 用于 Batch 完成追踪

	int32 Count = 0;
	int32 ChunkIndex = INDEX_NONE;

	FBulletHoleBatch() = default;
	~FBulletHoleBatch() = default;

	void Reserve(int32 Capacity)
	{
		ToolTransforms.Reserve(Capacity);
		Attempts.Reserve(Capacity);
		bIsPenetrations.Reserve(Capacity);
		TemporaryDecals.Reserve(Capacity);
		ToolMeshPtrs.Reserve(Capacity);
		CompletionBatchIds.Reserve(Capacity);
	}

	void Reset()
	{
		Count = 0;
		ToolTransforms.Reset();
		Attempts.Reset();
		bIsPenetrations.Reset();
		TemporaryDecals.Reset();
		ToolMeshPtrs.Reset();
		CompletionBatchIds.Reset();
	}

	void Add(FBulletHole& Op)
	{
		ToolTransforms.Add(Op.ToolTransform);
		Attempts.Add(Op.Attempts);
		bIsPenetrations.Add(Op.bIsPenetration);
		TemporaryDecals.Add(Op.TemporaryDecal);
		ToolMeshPtrs.Add(Op.ToolMeshPtr);
		// 仅添加有效的 BatchId（排除 INDEX_NONE）
		if (Op.BatchId != INDEX_NONE)
		{
			CompletionBatchIds.AddUnique(Op.BatchId);
		}
		Count++;
	}

	void Add(FBulletHole&& Op)
	{
		ToolTransforms.Add(MoveTemp(Op.ToolTransform));
		Attempts.Add(MoveTemp(Op.Attempts));
		bIsPenetrations.Add(MoveTemp(Op.bIsPenetration));
		TemporaryDecals.Add(MoveTemp(Op.TemporaryDecal));
		ToolMeshPtrs.Add(MoveTemp(Op.ToolMeshPtr));
		// 仅添加有效的 BatchId（排除 INDEX_NONE）
		if (Op.BatchId != INDEX_NONE)
		{
			CompletionBatchIds.AddUnique(Op.BatchId);
		}
		Count++;
	}

	bool Get(FBulletHole& OutOp, int32 Index)
	{
		if (Index >= Count)
		{
			return false;
		}

		OutOp.ToolTransform = MoveTemp(ToolTransforms[Index]);
		OutOp.Attempts = MoveTemp(Attempts[Index]);
		OutOp.bIsPenetration = MoveTemp(bIsPenetrations[Index]);
		OutOp.TemporaryDecal = MoveTemp(TemporaryDecals[Index]);
		OutOp.ToolMeshPtr = MoveTemp(ToolMeshPtrs[Index]);

		return true;
	}

	/*
	 * 移动操作会转移所有权，导致 ToolTransforms.Num() 归零，
	 * 移动后请勿依赖 ToolTransforms.Num() 获取元素数量。
	 */
	int32 Num() const { return Count; }
};

/** 单个 Chunk 的简化调度计数器与累计耗时统计 */
struct FChunkState
{
	int32 Interval = 0;
	int32 LastSimplifyTriCount = 0;
	int32 DurationAccumCount = 0;
	float SubtractDurationAccum = 0.0f;

	void Reset()
	{
		Interval = 0;
		DurationAccumCount = 0;
		SubtractDurationAccum = 0.0f;
	}
};

/** 包含所有 Chunk 处理状态的容器，提供生命周期管理辅助接口 */
struct FChunkProcessState
{
	TArray<FChunkState> States;
	
	void Initialize(int32 ChunkNum)
	{
		States.SetNumZeroed(ChunkNum);
	}

	void Reset()
	{
		for (FChunkState& State : States)
		{
			State.Reset();
		}
	}

	void Shutdown()
	{
		States.Empty();
	}

	FChunkState& GetState(int32 ChunkIndex)
	{
		check(States.IsValidIndex(ChunkIndex));
		return States[ChunkIndex];
	}
};

/**
 * 跨 Chunk 调度实时 Boolean 操作，支持批处理和异步 Worker。
 * 追踪每个 Chunk 的性能指标以自适应调整 Union 批量大小和简化间隔，
 * 最终在 Game Thread 上应用处理结果。
 */
class FRealtimeBooleanProcessor
{
public:
	/** 异步 Worker 用于安全检测 Processor 关闭状态的共享生命周期 Token */
	struct FProcessorLifeTime
	{
		std::atomic<bool> bAlive{ true };
		TWeakPtr<FRealtimeBooleanProcessor, ESPMode::ThreadSafe> Processor = nullptr;

		FProcessorLifeTime() = default;
		~FProcessorLifeTime()
		{
			Clear();
		}

		void Init(const TSharedPtr<FRealtimeBooleanProcessor, ESPMode::ThreadSafe>& Owner)
		{
			Processor = Owner;
			bAlive.store(true);
		}
		void Clear()
		{
			bAlive = false;
			Processor.Reset();
		}
	};

public:
	FRealtimeBooleanProcessor() = default;
	~FRealtimeBooleanProcessor();

	bool Initialize(URealtimeDestructibleMeshComponent* Owner);
	void Shutdown();

	/** 将 Boolean 操作请求入队 */
	void EnqueueOp(FRealtimeDestructionOp&& Operation, UDecalComponent* TemporaryDecal, UDynamicMeshComponent* ChunkMesh = nullptr, int32 BatchId = -1);
	/** 将剩余请求（含重试）重新入队 */
	void EnqueueRemaining(FBulletHole&& Operation);
	void EnqueueIslandRemoval(int32 ChunkIndex, TSharedPtr<UE::Geometry::FDynamicMesh3> ToolMesh, TSharedPtr<UE::Geometry::FDynamicMesh3> DebrisToolMesh, TSharedPtr<FIslandRemovalContext> Context);

	/**
	 * 从队列中构建每个 Chunk 的批次，并在有任务时启动 Worker。
	 * 单 Worker 模式下，仅在 Chunk 空闲时启动；否则批次延迟到下一帧重试。
	 */
	void KickProcessIfNeededPerChunk();

	/** 检查拥有者 URealtimeDestructibleMeshComponent 是否有效 */
	bool IsOwnerCompValid() const { return OwnerComponent.IsValid(); }

	/** 清除所有待处理任务并重置累计计数器 */
	void CancelAllOperations();

	/** 返回指定 Chunk 索引的孔洞数量 */
	int32 GetChunkHoleCount(int32 ChunkIndex) const { return ChunkHoleCount[ChunkIndex]; }
	/** 从组件解析 Chunk 索引并返回对应孔洞数量 */
	int32 GetChunkHoleCount(const UPrimitiveComponent* ChunkComponent) const;

	/** 执行网格 Boolean 运算，将结果写入 OutputMesh */
	static bool ApplyMeshBooleanAsync(const UE::Geometry::FDynamicMesh3* TargetMesh,
		const UE::Geometry::FDynamicMesh3* ToolMesh,
		UE::Geometry::FDynamicMesh3* OutputMesh,
		const EGeometryScriptBooleanOperation Operation,
		const FGeometryScriptMeshBooleanOptions Options,
		const FTransform& TargetTransform = FTransform::Identity,
		const FTransform& ToolTransform = FTransform::Identity
		);

	/**
	 * 对网格执行平面简化，去除低权重顶点以防止三角面数量爆炸。
	 */
	static void ApplySimplifyToPlanarAsync(UE::Geometry::FDynamicMesh3* TargetMesh,
		FGeometryScriptPlanarSimplifyOptions Options,
		bool bEnableDetail);

	/**
	 * 执行均匀重网格化以降低累积的顶点数量。
	 * 边界边完全约束，确保网格轮廓不变形。
	 * @param TargetMesh 原地重网格化的目标网格。
	 * @param TargetEdgeLength 重网格化后的目标边长。
	 * @param NumPasses 重网格迭代次数（越多收敛越好）。
	 */
	static void ApplyUniformRemesh(UE::Geometry::FDynamicMesh3* TargetMesh, double TargetEdgeLength, int32 NumPasses = 5);

private:
	// ===============================================================
	// 处理管线
	// ===============================================================
	void StartBooleanWorkerAsyncForChunk(FBulletHoleBatch&& InBatch, int32 Gen);
	void EnqueueRetryOps(TQueue<FBulletHole, EQueueMode::Mpsc>& Queue, FBulletHoleBatch&& InBatch,
		UDynamicMeshComponent* TargetMesh, int32 ChunkIndex, int32& DebugCount);
	int32& GetChunkInterval(int32 ChunkIndex);

	// ===============================================================
	// 简化与自适应调优
	// ===============================================================
	void AccumulateSubtractDuration(int32 ChunkIndex, double CurrentSubDuration);
	void UpdateSimplifyInterval(double CurrentSetMeshAvgCost, int32 ChunkIndex);
	void UpdateUnionSize(int32 ChunkIndex, double DurationMs);
	bool TrySimplify(UE::Geometry::FDynamicMesh3& WorkMesh, int32 ChunkIndex, int32 UnionCount, bool bEnableDetail);
	// 更新 Subtract 平均耗时统计
	void UpdateSubtractAvgCost(double CostMs);

	// ===============================================================
	// 线程管理与 Slot Worker
	// ===============================================================
	// 获取 ThreadManager 的辅助方法
	URDMThreadManagerSubsystem* GetThreadManager() const;

	void InitializeSlots();
	void ShutdownSlots();

	// 找到最空闲的 Slot
	int32 FindLeastBusySlot() const;

	// 启动 Worker
	void KickUnionWorker(int32 SlotIndex);
	void KickSubtractWorker(int32 SlotIndex);

	// Worker 主循环（Batch 作为参数传入以保证 MPSC 队列安全性）
	void ProcessSlotUnionWork(int32 SlotIndex, FBulletHoleBatch&& Batch);
	void ProcessSlotSubtractWork(int32 SlotIndex, FUnionResult&& UnionResult);

	// Slot 耗尽时清理映射关系
	void CleanupSlotMapping(int32 SlotIndex);

	void BooleanOpSync(FBulletHole&& Op);

private:
	// ===============================================================
	// 处理管线
	// ===============================================================
	TWeakObjectPtr<URealtimeDestructibleMeshComponent> OwnerComponent = nullptr;

	TSharedPtr<FProcessorLifeTime, ESPMode::ThreadSafe> LifeTime;

	// 穿透与非穿透操作使用独立队列
	TQueue<FBulletHole, EQueueMode::Mpsc> HighPriorityQueue;
	int DebugHighQueueCount;

	TQueue<FBulletHole, EQueueMode::Mpsc> NormalPriorityQueue;
	int DebugNormalQueueCount;

	FChunkProcessState ChunkStates;

	// Chunk 代次追踪：每次将 Boolean 结果应用到网格时递增
	TArray<std::atomic<int32>> ChunkGenerations;

	/** 每个 Chunk 独立的 Union 结果队列（各 Chunk 管线相互独立） */
	TArray<TUniquePtr<TQueue<FUnionResult, EQueueMode::Mpsc>>> ChunkUnionResultsQueues;

	/** 每个 Chunk 的 Batch ID 计数器 */
	TArray<std::atomic<int32>> ChunkNextBatchIDs;

	// 每个 Chunk 允许的最大 Union Tool Mesh 数量
	TArray<uint8> MaxUnionCount;

	TArray<int32> ChunkHoleCount = {};

	bool bEnableMultiWorkers;
	std::atomic<int32> ActiveChunkCount{ 0 };

	TArray<TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe>> CachedChunkMeshes;

	// ===============================================================
	// 简化与自适应调优
	// ===============================================================
	// 使用默认值进行测试
	float AngleThreshold = 0.001;

	TArray<uint16> MaxInterval = {};
	uint8 InitInterval = 0;

	double SubDurationHighThreshold = 0.0;
	double SubDurationLowThreshold = 5.0;

	TArray<double> SetMeshAvgCost = {};

	double FrameBudgetMs = 16.0f;
	double SubtractAvgCostMs = 2.0f;
	double SubtractCostAccum = 0.0f;
	int32 SubtractCostSampleCount = 0;

	// ===============================================================
	// 线程管理与 Slot Worker
	// ===============================================================
	// Slot 数量（Worker 管理槽位数）
	int32 NumSlots = 1;

	int32 MaxUnionWorkerPerSlot = 1;
	int32 MaxSubtractWorkerPerSlot = 3;

	// 仅用于调试/统计
	TArray<TUniquePtr<std::atomic<int32>>> SlotUnionWorkerCounts;
	TArray<TUniquePtr<std::atomic<int32>>> SlotSubtractWorkerCounts;

	// 每个 Slot 的 Union 队列
	TArray<TUniquePtr<TQueue<FBulletHoleBatch, EQueueMode::Mpsc>>> SlotUnionQueues;

	// 每个 Slot 的 Subtract 队列
	TArray<TUniquePtr<TQueue<FUnionResult, EQueueMode::Mpsc>>> SlotSubtractQueues;

	FCriticalSection MapLock;

	// 每个 Slot 的 Worker 激活标志
	TArray<TUniquePtr<std::atomic<bool>>> SlotUnionActiveFlags;
	TArray<TUniquePtr<std::atomic<bool>>> SlotSubtractActiveFlags;
};




