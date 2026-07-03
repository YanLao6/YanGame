// Copyright (c) 2026 LazyDevelopers <lazydeveloper24@gmail.com>. All rights reserved.
// This plugin is distributed under the Fab Standard License.
//
// This product was independently developed by us while participating in the Epic Project, a developer-support
// program of the KRAFTON JUNGLE GameTech Lab. All rights, title, and interest in and to the product are exclusively
// vested in us. Krafton, Inc. was not involved in its development and distribution and disclaims all representations
// and warranties, express or implied, and assumes no responsibility or liability for any consequences arising from
// the use of this product.

#include "RealtimeBooleanProcessor.h"
#include "MeshSimplification.h"
#include "Tasks/Task.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Components/RealtimeDestructibleMeshComponent.h"
#include "DynamicMesh/MeshTransforms.h"
#include "Operations/MeshBoolean.h"
#include "Operations/MinimalHoleFiller.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Components/DecalComponent.h"
#include "GeometryScript/MeshSimplifyFunctions.h"
#include "ProfilingDebugging/CountersTrace.h"
// 仅用于调试
#include "DebugConsoleVariables.h"
#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"
#include "HAL/CriticalSection.h"
#include "Subsystems/RDMThreadManagerSubsystem.h"
#include "Remesher.h"
#include "MeshConstraintsUtil.h"
#include "Actors/DebrisActor.h"
#include "Operations/MeshClusterSimplifier.h"
#include "Debug/DebugConsoleVariables.h"

TRACE_DECLARE_INT_COUNTER(Counter_ThreadCount, TEXT("RealtimeDestruction/ThreadCount"));
TRACE_DECLARE_INT_COUNTER(Counter_UnionThreadCount, TEXT("RealtimeDestruction/UnionThreadCount"));
TRACE_DECLARE_INT_COUNTER(Counter_SubtractWorkerCount, TEXT("RealtimeDestruction/SubtractThreadCount"));
TRACE_DECLARE_INT_COUNTER(Counter_ActiveChunks, TEXT("RealtimeDestruction/ActiveChunks"));

TRACE_DECLARE_FLOAT_COUNTER(Counter_Throughput, TEXT("RealtimeDestruction/Throughput"));
TRACE_DECLARE_INT_COUNTER(Counter_BatchSize, TEXT("RealtimeDestruction/BatchSize"));
TRACE_DECLARE_FLOAT_COUNTER(Counter_WorkTime, TEXT("RealtimeDestruction/WorkTimeMs"));

using namespace UE::Geometry;

FRealtimeBooleanProcessor::~FRealtimeBooleanProcessor()
{
	OwnerComponent.Reset();
	if (LifeTime.IsValid())
	{
		LifeTime->Clear();
	}
}

bool FRealtimeBooleanProcessor::Initialize(URealtimeDestructibleMeshComponent* Owner)
{
	if (!Owner)
	{
		return false;
	}

	OwnerComponent = Owner;
	OwnerComponent->SettingAsyncOption(bEnableMultiWorkers);

	InitInterval = OwnerComponent->GetInitInterval();

	int32 ChunkNum = OwnerComponent->GetChunkNum();
	if (ChunkNum > 0)
	{
		ChunkGenerations.SetNumZeroed(ChunkNum);
		ChunkStates.Initialize(ChunkNum);
		ChunkHoleCount.SetNumZeroed(ChunkNum);
		MaxInterval.Init(InitInterval, ChunkNum);
		SetMeshAvgCost.SetNumZeroed(ChunkNum);

		// 从初始值10开始
		MaxUnionCount.Init(10, ChunkNum);

		// 初始化块多线程工作者状态
		ChunkUnionResultsQueues.SetNum(ChunkNum);
		
		for (int32 i = 0; i < ChunkNum; ++i)
		{
			ChunkUnionResultsQueues[i] = MakeUnique<TQueue<FUnionResult, EQueueMode::Mpsc>>();
			
			// 为块状态设置LastSimplifyTriCount
			FDynamicMesh3 ChunkMesh;
			OwnerComponent->GetChunkMesh(ChunkMesh, i);
			ChunkStates.States[i].LastSimplifyTriCount = ChunkMesh.TriangleCount();
		}

		ChunkNextBatchIDs.SetNumZeroed(ChunkNum); 
	}

	LifeTime = MakeShared<FProcessorLifeTime, ESPMode::ThreadSafe>();
	LifeTime->Init(Owner->GetBooleanProcessorShared());	

	AngleThreshold = OwnerComponent->GetAngleThreshold();
	SubDurationHighThreshold = OwnerComponent->GetSubtractDurationLimit();	

	InitializeSlots();

	return true;
}

void FRealtimeBooleanProcessor::Shutdown()
{
	if (!LifeTime.IsValid())
	{
		return;
	}

	if (OwnerComponent.IsValid())
	{
		OwnerComponent.Reset();
	}

	LifeTime->Clear();
	LifeTime.Reset();

	FBulletHole Temp;
	while (HighPriorityQueue.Dequeue(Temp)) {}
	while (NormalPriorityQueue.Dequeue(Temp)) {}

	DebugHighQueueCount = 0;
	DebugNormalQueueCount = 0;

	// 清理块队列
	for (auto& Queue : ChunkUnionResultsQueues)
	{
		if (Queue)
		{
			FUnionResult TempResult;
			while (Queue->Dequeue(TempResult)) {} 
		}
	}

	ChunkUnionResultsQueues.Empty();
	ChunkNextBatchIDs.Empty(); 

	ChunkGenerations.Empty();

	ChunkStates.Shutdown();

	ShutdownSlots();
}

void FRealtimeBooleanProcessor::EnqueueOp(FRealtimeDestructionOp&& Operation, UDecalComponent* TemporaryDecal, UDynamicMeshComponent* ChunkMesh, int32 BatchId)
{
	if (!OwnerComponent.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("OwnerComponent is invalid"));
		if (auto Owner = OwnerComponent.Get())
		{
			Owner->NotifyBooleanSkipped(BatchId);
		}
		return;
	}

	if (!ChunkMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("Chunk is null"));
		OwnerComponent->NotifyBooleanSkipped(BatchId);
		return;
	}

	FBulletHole Op = {};
	Op.ChunkIndex = Operation.Request.ChunkIndex;
	Op.BatchId = BatchId;
	Op.TargetMesh = ChunkMesh;
	FTransform ComponentToWorld = Op.TargetMesh->GetComponentTransform();

	const FVector LocalImpact = ComponentToWorld.InverseTransformPosition(Operation.Request.ToolOriginWorld);

	// 缩放校正：在旋转后的框架中计算轴缩放。
	const FVector ComponentScale = ComponentToWorld.GetScale3D();

	switch (Operation.Request.ToolShape)
	{
	case EDestructionToolShape::Cylinder:
	{
		const FVector LocalNormal = ComponentToWorld.InverseTransformVector(Operation.Request.ToolForwardVector).GetSafeNormal();
		FQuat ToolRotation = FRotationMatrix::MakeFromZ(LocalNormal).ToQuat(); // 圆柱体和圆锥体必须旋转以匹配方向。

		// 在组件局部空间中旋转后的工具网格局部轴。
		FVector ToolAxisX = ToolRotation.RotateVector(FVector::XAxisVector);
		FVector ToolAxisY = ToolRotation.RotateVector(FVector::YAxisVector);
		FVector ToolAxisZ = ToolRotation.RotateVector(FVector::ZAxisVector);

		// 根据ComponentScale计算轴拉伸。
		FVector ScaledAxisX = ToolAxisX * ComponentScale;
		FVector ScaledAxisY = ToolAxisY * ComponentScale;
		FVector ScaledAxisZ = ToolAxisZ * ComponentScale;

		// 调整后的缩放：恢复到原始大小。
		FVector AdjustedScale = FVector(
			1.0f / FMath::Max(KINDA_SMALL_NUMBER, ScaledAxisX.Size()),
			1.0f / FMath::Max(KINDA_SMALL_NUMBER, ScaledAxisY.Size()),
			1.0f / FMath::Max(KINDA_SMALL_NUMBER, ScaledAxisZ.Size())
		);

		Op.ToolTransform = FTransform(ToolRotation, LocalImpact, AdjustedScale);
		break;
	}
	case EDestructionToolShape::Sphere:
	{ 
		  FVector InverseScale = FVector(
		  1.0f / FMath::Max(KINDA_SMALL_NUMBER, ComponentScale.X),
		  1.0f / FMath::Max(KINDA_SMALL_NUMBER, ComponentScale.Y),
		  1.0f / FMath::Max(KINDA_SMALL_NUMBER, ComponentScale.Z)
		);

      Op.ToolTransform = FTransform(FQuat::Identity, LocalImpact, InverseScale);
		break;
	}
	default:
		break;
	}
	  
	Op.bIsPenetration = Operation.bIsPenetration;
	Op.TemporaryDecal = TemporaryDecal;
	Op.ToolMeshPtr = Operation.Request.ToolMeshPtr;

	if (FRDMCVarHelper::EnableAsyncBooleanOp())
	{
	UE_LOG(LogTemp, Warning, TEXT("High Queue Size: %d"), DebugHighQueueCount);
	UE_LOG(LogTemp, Warning, TEXT("Normal Queue Size: %d"), DebugNormalQueueCount);

	if (Op.bIsPenetration)
	{
		HighPriorityQueue.Enqueue(MoveTemp(Op));
		DebugHighQueueCount++;
		UE_LOG(LogTemp, Warning, TEXT("[Enqueue] ✅ High Priority Queue Size: %d"), DebugHighQueueCount);
	}
	else
	{
		NormalPriorityQueue.Enqueue(MoveTemp(Op));
		DebugNormalQueueCount++;
		UE_LOG(LogTemp, Warning, TEXT("[Enqueue] ✅ Normal Priority Queue Size: %d"), DebugNormalQueueCount);
	}	
	}
	else
	{
		BooleanOpSync(MoveTemp(Op));
	}
}

void FRealtimeBooleanProcessor::EnqueueRemaining(FBulletHole&& Operation)
{
	if (Operation.bIsPenetration)
	{
		HighPriorityQueue.Enqueue(MoveTemp(Operation));
		DebugHighQueueCount++;
	}
	else
	{
		NormalPriorityQueue.Enqueue(MoveTemp(Operation));
		DebugNormalQueueCount++;
	}
}

void FRealtimeBooleanProcessor::EnqueueIslandRemoval(
	int32 ChunkIndex,
	TSharedPtr<UE::Geometry::FDynamicMesh3> ToolMesh,
	TSharedPtr<UE::Geometry::FDynamicMesh3> DebrisToolMesh,
	TSharedPtr<FIslandRemovalContext> Context)
{
	if (ChunkIndex == INDEX_NONE)
	{
		return;
	}

	// ToolMesh 또는 DebrisToolMesh 중 하나라도 있어야 함
	// Client extraction의 경우 ToolMesh는 nullptr이고 DebrisToolMesh만 있음
	if (!ToolMesh.IsValid() && !DebrisToolMesh.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnqueueIslandRemoval] Both ToolMesh and DebrisToolMesh are invalid - skipping"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[EnqueueIslandRemoval] ChunkIndex=%d, ToolMesh=%d, DebrisToolMesh=%d, TargetDebrisActor=%d"),
		ChunkIndex, ToolMesh.IsValid() ? 1 : 0, DebrisToolMesh.IsValid() ? 1 : 0,
		(Context.IsValid() && Context->TargetDebrisActor.IsValid()) ? 1 : 0);

	FUnionResult WorkItem = {};
	WorkItem.ChunkIndex = ChunkIndex;
	WorkItem.SharedToolMesh = ToolMesh;
	WorkItem.DebrisSharedToolMesh = DebrisToolMesh;
	WorkItem.OutDebrisMesh = MakeShared<FDynamicMesh3>();
	WorkItem.WorkType = EBooleanWorkType::IslandRemoval;
	WorkItem.IslandContext = Context;
	
	WorkItem.Decals = {};
	WorkItem.PendingCombinedToolMesh = {};
	WorkItem.UnionCount = 0;

	int32 SlotIndex = FindLeastBusySlot();
	if (SlotSubtractQueues.IsValidIndex(SlotIndex))
	{
		SlotSubtractQueues[SlotIndex]->Enqueue(MoveTemp(WorkItem));
		KickSubtractWorker(SlotIndex);
	}
}

void FRealtimeBooleanProcessor::UpdateSubtractAvgCost(double CostMs)
{
	SubtractCostAccum += CostMs;
	SubtractCostSampleCount++;

	if (SubtractCostSampleCount >= 10)
	{
		SubtractAvgCostMs = SubtractCostAccum / SubtractCostSampleCount;
		SubtractCostAccum = SubtractAvgCostMs;
		SubtractCostSampleCount = 1;
	}
}

URDMThreadManagerSubsystem* FRealtimeBooleanProcessor::GetThreadManager() const
{
	if (!OwnerComponent.IsValid())
	{
		return nullptr;
	}
	UWorld* World = OwnerComponent->GetWorld();
	return URDMThreadManagerSubsystem::Get(World);
}

void FRealtimeBooleanProcessor::InitializeSlots()
{
	if (URDMThreadManagerSubsystem* ThreadManager = GetThreadManager())
	{
		NumSlots = ThreadManager->GetSlotCount();
	}
	else
	{
		NumSlots = 1;
	}
	
	// 创建联合队列。
	SlotUnionQueues.SetNum(NumSlots);
	for (int32 i = 0; i < NumSlots; ++i)
	{
		SlotUnionQueues[i] = MakeUnique<TQueue<FBulletHoleBatch, EQueueMode::Mpsc>>();
	}

	// 创建减法队列。
	SlotSubtractQueues.SetNum(NumSlots);
	for (int32 i = 0; i < NumSlots; ++i)
	{
		SlotSubtractQueues[i] = MakeUnique<TQueue<FUnionResult, EQueueMode::Mpsc>>();
	} 

	// 创建活动标志。
	SlotUnionActiveFlags.SetNum(NumSlots);
	SlotSubtractActiveFlags.SetNum(NumSlots);
	SlotUnionWorkerCounts.SetNum(NumSlots);     
	SlotSubtractWorkerCounts.SetNum(NumSlots);
	for (int32 i = 0; i < NumSlots; ++i)
	{
		SlotUnionActiveFlags[i] = MakeUnique<std::atomic<bool>>(false);
		SlotSubtractActiveFlags[i] = MakeUnique<std::atomic<bool>>(false);

		
		SlotUnionWorkerCounts[i] = MakeUnique<std::atomic<int32>>(0);
		SlotSubtractWorkerCounts[i] = MakeUnique<std::atomic<int32>>(0);
	}	 
}

void FRealtimeBooleanProcessor::ShutdownSlots()
{
	// 清理联合队列。
	  for (auto& Queue : SlotUnionQueues)
	  {
	  	if (Queue)
	  	{
	  		FBulletHoleBatch Dummy;
	  		while (Queue->Dequeue(Dummy)) {} 
	  	}
	  }
	SlotUnionQueues.Empty();

	// 清理减法队列。
	for (auto& Queue : SlotSubtractQueues)
	{
		if (Queue)
		{
			FUnionResult Dummy;
			while (Queue->Dequeue(Dummy)) {} 
		}
	}
	SlotSubtractQueues.Empty();
   
	// 清理标志。
	SlotUnionActiveFlags.Empty();
	 
	SlotSubtractActiveFlags.Empty(); 
	 
	SlotUnionWorkerCounts.Empty();   

	SlotSubtractWorkerCounts.Empty();
}

int32 FRealtimeBooleanProcessor::FindLeastBusySlot() const
{
	int32 BestSlot = 0;
	int32 MinScore = INT32_MAX;

	for (int32 i = 0; i < NumSlots; i++)
	{
		// 基于活动工作线程计数的得分。
		int32 Score = SlotUnionWorkerCounts[i]->load() + SlotSubtractWorkerCounts[i]->load() * 2;

		if (Score < MinScore)
		{
			MinScore = Score;
			BestSlot = i;
		}
	}

	return BestSlot;
}

void FRealtimeBooleanProcessor::KickUnionWorker(int32 SlotIndex)
{
	// 首先出队（仅在GameThread上安全）。
	FBulletHoleBatch Batch;
	if (!SlotUnionQueues[SlotIndex]->Dequeue(Batch))
	{
		return;  // 队列为空。
	} 

	// 检查每个插槽的工作线程限制。
	int32 Current = SlotUnionWorkerCounts[SlotIndex]->fetch_add(1);
	if (Current >= MaxUnionWorkerPerSlot)
	{
        SlotUnionWorkerCounts[SlotIndex]->fetch_sub(1);
		SlotUnionQueues[SlotIndex]->Enqueue(Batch);
		return;
	}
	
	// 获取ThreadManager。
	URDMThreadManagerSubsystem* ThreadManager = GetThreadManager();
	if (!ThreadManager)
	{
        SlotUnionWorkerCounts[SlotIndex]->fetch_sub(1);
		SlotUnionQueues[SlotIndex]->Enqueue(MoveTemp(Batch));
		return;
	}

	
	// 4. 启动工作线程（捕获批次）。
	UE_LOG(LogTemp, Log, TEXT("[Slot %d] Union Worker Started: %d / %d"),
		SlotIndex, SlotUnionWorkerCounts[SlotIndex]->load() , MaxUnionWorkerPerSlot);

	TSharedPtr<FProcessorLifeTime, ESPMode::ThreadSafe> LifeTimeToken = LifeTime;
	ThreadManager->RequestWork(
		[LifeTimeToken, SlotIndex, Batch = MoveTemp(Batch)]() mutable
		{
			if (!LifeTimeToken.IsValid() || !LifeTimeToken->bAlive.load())
			{
				return;
			}
			if (auto Processor = LifeTimeToken->Processor.Pin())
			{
				Processor->ProcessSlotUnionWork(SlotIndex, MoveTemp(Batch));
			}			
		},
		OwnerComponent.Get()
	);	
}

void FRealtimeBooleanProcessor::KickSubtractWorker(int32 SlotIndex)
{
	// 首先出队（仅在GameThread上安全）。
	FUnionResult UnionResult;
	if (!SlotSubtractQueues[SlotIndex]->Dequeue(UnionResult))
	{
		return;  // 队列为空。
	}

	// 为每个插槽保留工作线程。
	int32 Current = SlotSubtractWorkerCounts[SlotIndex]->fetch_add(1);
	if (Current >= MaxSubtractWorkerPerSlot)
	{
		// 达到最大值 -> 回滚并重新将UnionResult入队。
		SlotSubtractWorkerCounts[SlotIndex]->fetch_sub(1);
		SlotSubtractQueues[SlotIndex]->Enqueue(MoveTemp(UnionResult));
		return;
	}

	// 获取ThreadManager。
	URDMThreadManagerSubsystem* ThreadManager = GetThreadManager();
	if (!ThreadManager)
	{ 
		SlotSubtractWorkerCounts[SlotIndex]->fetch_sub(1);
		SlotSubtractQueues[SlotIndex]->Enqueue(MoveTemp(UnionResult));
		return;
	}

	// 启动工作线程（捕获UnionResult）。
	UE_LOG(LogTemp, Log, TEXT("[Slot %d] Subtract Worker Started: %d / %d"),
		SlotIndex, SlotSubtractWorkerCounts[SlotIndex]->load(), MaxSubtractWorkerPerSlot);

	TSharedPtr<FProcessorLifeTime, ESPMode::ThreadSafe> LifeTimeToken = LifeTime;
	if (OwnerComponent.IsValid() && !OwnerComponent->CheckAndSetChunkBusy(UnionResult.ChunkIndex))
	{
		ThreadManager->RequestWork(
		   [LifeTimeToken, SlotIndex, UnionResult = MoveTemp(UnionResult)]() mutable
		   {
		   	if (!LifeTimeToken.IsValid() || !LifeTimeToken->bAlive.load())
		   	{
		   		return;
		   	}
		   	if (auto Processor = LifeTimeToken->Processor.Pin())
		   	{
		   		Processor->ProcessSlotSubtractWork(SlotIndex, MoveTemp(UnionResult));
		   	}
		   },
		   OwnerComponent.Get()
	   );
	}
	else
	{
		SlotSubtractWorkerCounts[SlotIndex]->fetch_sub(1);
		UE_LOG(LogTemp, Warning, TEXT("Re-enqueued due to ChunkBusy! ChunkIndex=%d, Context=%p"),
			UnionResult.ChunkIndex, UnionResult.IslandContext.Get());
		SlotSubtractQueues[SlotIndex]->Enqueue(MoveTemp(UnionResult));
	}
}

void FRealtimeBooleanProcessor::ProcessSlotUnionWork(int32 SlotIndex, FBulletHoleBatch&& Batch)
{
	// 批处理作为参数传递（无出队）。
	int32 ChunkIndex = Batch.ChunkIndex;
	if (ChunkIndex == INDEX_NONE)
	{
		SlotUnionWorkerCounts[SlotIndex]->fetch_sub(1);
		return;
	}

	// 执行联合（无块网格访问；仅工具网格）。
	FDynamicMesh3 CombinedToolMesh;
	TArray<TWeakObjectPtr<UDecalComponent>> Decals;
	int32 UnionCount = 0;

	int32 BatchCount = Batch.Num();
	TArray<FTransform> ToolTransforms = MoveTemp(Batch.ToolTransforms);
	TArray<TWeakObjectPtr<UDecalComponent>> TemporaryDecals = MoveTemp(Batch.TemporaryDecals);
	TArray<TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe>> ToolMeshPtrs = MoveTemp(
		Batch.ToolMeshPtrs);

	bool bIsFirst = true;
	for (int32 i = 0; i < BatchCount; ++i)
	{
		if (!ToolMeshPtrs[i].IsValid())
		{
			continue;
		}

		FTransform ToolTransform = MoveTemp(ToolTransforms[i]);
		TWeakObjectPtr<UDecalComponent> TemporaryDecal = MoveTemp(TemporaryDecals[i]);

		// 跳过空网格（避免崩溃）。
		FDynamicMesh3 CurrentTool = *(ToolMeshPtrs[i]);
		if (CurrentTool.TriangleCount() == 0)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT(
				       "[UnionWorkerForChunk] Skipping empty ToolMesh at ChunkIndex %d, item %d"
			       ), ChunkIndex, i);
			continue;
		}

		//FDynamicMesh3 CurrentTool = MoveTemp(*(ToolMeshPtrs[i]));
		MeshTransforms::ApplyTransform(CurrentTool, (FTransformSRT3d)ToolTransform, true);

		if (TemporaryDecal.IsValid())
		{
			Decals.Add(TemporaryDecal);
		}

		if (bIsFirst)
		{
			CombinedToolMesh = MoveTemp(CurrentTool);
			bIsFirst = false;
			UnionCount++;
		}
		else
		{
			FDynamicMesh3 UnionResult;
			FMeshBoolean MeshUnion(
				&CombinedToolMesh, FTransform::Identity,
				&CurrentTool, FTransform::Identity,
				&UnionResult, FMeshBoolean::EBooleanOp::Union
			);

			bool bUnionSuccess = false;
			{
#if !UE_BUILD_SHIPPING
				TRACE_CPUPROFILER_EVENT_SCOPE("SlotWorkerUnion_Union");
#endif
				bUnionSuccess = MeshUnion.Compute();
			}

			if (bUnionSuccess)
			{
				CombinedToolMesh = MoveTemp(UnionResult);
				UnionCount++;

				UE_LOG(LogTemp, Display, TEXT("ToolMeshTri %d"), CombinedToolMesh.TriangleCount());
				
			}
			else
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("[UnionWorkerForChunk] Union failed at ChunkIndex %d, item %d"),
				       ChunkIndex, i);
			}
		}
	}

	if (UnionCount > 0 && CombinedToolMesh.TriangleCount() > 0)
	{
		FUnionResult Result;
		//Result.BatchID = BatchID;
		Result.PendingCombinedToolMesh = MoveTemp(CombinedToolMesh);
		Result.Decals = MoveTemp(Decals);
		Result.UnionCount = UnionCount;
		Result.ChunkIndex = ChunkIndex;
		// 复制用于批处理完成跟踪的ID数组
		Result.CompletionBatchIds = MoveTemp(Batch.CompletionBatchIds);

		// 入队到减法队列。
		SlotSubtractQueues[SlotIndex]->Enqueue(MoveTemp(Result));
	}

	SlotUnionWorkerCounts[SlotIndex]->fetch_sub(1);

	// 启动减法（在GameThread上）。
	AsyncTask(ENamedThreads::GameThread, [LifeTimeToken = LifeTime, SlotIndex]()
	{
		if (!LifeTimeToken.IsValid() || !LifeTimeToken->bAlive.load())
		{
			return;
		}

		if (auto Processor = LifeTimeToken->Processor.Pin())
		{
			// 如果队列有工作，再次启动。
			//if (!Processor->SlotUnionQueues[SlotIndex]->IsEmpty())
			//{
			//	Processor->KickUnionWorker(SlotIndex);
			//}

			for (int32 i = 0; i < Processor->SlotUnionQueues.Num(); i++)
			{
				if (!Processor->SlotUnionQueues[i]->IsEmpty())
				{
					Processor->KickUnionWorker(i);
				}
			}
			// 也启动减法工作线程。
			//Processor->KickSubtractWorker(SlotIndex);
			 for (int32 i = 0; i < Processor->SlotSubtractQueues.Num(); i++)
              {
                  if (!Processor->SlotSubtractQueues[i]->IsEmpty())
                  {
                      Processor->KickSubtractWorker(i);
                  }
              }
		}
	});
}

void FRealtimeBooleanProcessor::ProcessSlotSubtractWork(int32 SlotIndex, FUnionResult&& UnionResult)
{
	auto HandleFailureAndReturn = [&]()
	{
		int32 ChunkIndex = UnionResult.ChunkIndex;
		SlotSubtractWorkerCounts[SlotIndex]->fetch_sub(1);

		AsyncTask(ENamedThreads::GameThread, [LifeTimeToken = LifeTime,
			          SlotIndex = SlotIndex,
			          ChunkIndex = ChunkIndex,
			          Context = UnionResult.IslandContext,
			          CompletionBatchIds = UnionResult.CompletionBatchIds]()  // 捕获以进行批处理完成跟踪
		          {
			          if (!LifeTimeToken.IsValid() || !LifeTimeToken->bAlive.load())
			          {
				          return;
			          }

			          TSharedPtr<FRealtimeBooleanProcessor, ESPMode::ThreadSafe> Processor = LifeTimeToken->Processor.Pin();
			          if (!Processor.IsValid())
			          {
				          return;
			          }

			          URealtimeDestructibleMeshComponent* Owner = Processor->OwnerComponent.Get();
			          if (!Owner)
			          {
				          return;
			          }

			          // 释放块繁忙位
			          Owner->ClearChunkBusy(ChunkIndex);

			          // 即使失败也要跟踪批处理完成：为所有BatchId通知完成
			          for (int32 BatchId : CompletionBatchIds)
			          {
				          Owner->NotifyBooleanCompleted(BatchId);
			          }

			          if (Context.IsValid())
			          {
				          if (Context->RemainingTaskCount.fetch_sub(1) == 1)
				          {
					          if (Context->AccumulatedDebrisMesh.TriangleCount() > 0)
					          {
						          TArray<UMaterialInterface*> Materials;
						          if (auto ChunkMesh = Owner->GetChunkMeshComponent(ChunkIndex))
						          {
							          for (int32 i = 0; i < ChunkMesh->GetNumMaterials(); i++)
							          {
								          Materials.Add(ChunkMesh->GetMaterial(i));
							          }
						          }

						          // 如果存在TargetDebrisActor，则将网格应用于现有Actor（客户端提取）
						          if (Context->TargetDebrisActor.IsValid())
						          {
									  Owner->SpawnDebrisActor(MoveTemp(Context->AccumulatedDebrisMesh), Materials, Context->TargetDebrisActor.Get());

						          }
						          // 否则：生成新的DebrisActor（独立/侦听服务器）
						          else if (Context->Owner.IsValid())
						          {
					          Context->Owner->SpawnDebrisActor(MoveTemp(Context->AccumulatedDebrisMesh), Materials);
				          }
			          }

					          // IslandRemoval完成后清理小碎片
					          if (Context->DisconnectedCellsForCleanup.Num() > 0 && Context->Owner.IsValid())
					          {
						          Context->Owner->CleanupSmallFragments(Context->DisconnectedCellsForCleanup);
					          }
				          }

				          // 减少IslandRemoval计数器（用于与布尔批处理完成协调）
				          if (Context->Owner.IsValid())
				          {
					          Context->Owner->DecrementIslandRemovalCount();
				          }
			          }

					  for (int i = 0; i < Processor->SlotSubtractQueues.Num(); ++i)
					  {
						  if (!Processor->SlotSubtractQueues[i]->IsEmpty())
						  {
							  Processor->KickSubtractWorker(i);
						  }
					  }
		          });
	};

	// UnionResult作为参数传递（无出队）。
	int32 ChunkIndex = UnionResult.ChunkIndex;
	if (ChunkIndex == INDEX_NONE)
	{
		SlotSubtractWorkerCounts[SlotIndex]->fetch_sub(1);
		// 如果队列有工作，重新启动。
		if (!SlotSubtractQueues[SlotIndex]->IsEmpty())
		{
			KickSubtractWorker(SlotIndex);
		}
		return;
	}

	// ===== 4. 减法计算 =====
	FDynamicMesh3 ResultMesh;
	bool bSuccess = false; 
	bool bHasDebris = false; 
	{	
		// 获取块网格。
		FDynamicMesh3 WorkMesh;
		if (!OwnerComponent->GetChunkMesh(WorkMesh, ChunkIndex))
		{
			HandleFailureAndReturn();
			return;
		}		

		if (WorkMesh.TriangleCount() == 0)
		{
			HandleFailureAndReturn();
			return;
		}

		if (UnionResult.WorkType == EBooleanWorkType::BulletHole)
		{
			if (UnionResult.PendingCombinedToolMesh.TriangleCount() == 0)
			{
				// SlotSubtractWorkerCounts[SlotIndex]->fetch_sub(1);
				// if (!SlotSubtractQueues[SlotIndex]->IsEmpty())
				// {
				// 	KickSubtractWorker(SlotIndex);
				// }

				HandleFailureAndReturn();
				return;
			}

			// 运行减法。
			FGeometryScriptMeshBooleanOptions Options = OwnerComponent->GetBooleanOptions();

			double CurrentSubtractDurationMs = FPlatformTime::Seconds();

			{
#if !UE_BUILD_SHIPPING
				TRACE_CPUPROFILER_EVENT_SCOPE("SlotWorkerUnion_Subtract");
#endif
				bSuccess = ApplyMeshBooleanAsync(
				   &WorkMesh,
				   &UnionResult.PendingCombinedToolMesh,
				   &ResultMesh,
				   EGeometryScriptBooleanOperation::Subtract,
				   Options);
			}

			CurrentSubtractDurationMs = (FPlatformTime::Seconds() - CurrentSubtractDurationMs) * 1000.0;

			if (bSuccess)
			{
				AccumulateSubtractDuration(ChunkIndex, CurrentSubtractDurationMs);     
				UpdateUnionSize(ChunkIndex, CurrentSubtractDurationMs);
				// 简化。
				bool bEnableDetailMode = OwnerComponent->IsHighDetailMode();
				{
#if !UE_BUILD_SHIPPING
					TRACE_CPUPROFILER_EVENT_SCOPE("SlotWorkerUnion_Simplify");
#endif
					TrySimplify(ResultMesh, ChunkIndex, UnionResult.UnionCount, bEnableDetailMode);
				}
			}
			else
			{
				UE_LOG(LogTemp, Display, TEXT("Reset Accumulation"));
				// 失败时重置累积。
				FChunkState& State = ChunkStates.GetState(ChunkIndex);
				State.SubtractDurationAccum = 0;
				State.DurationAccumCount = 0;
			}
		}
		else
		{
			FGeometryScriptMeshBooleanOptions Ops;
			Ops.bFillHoles = true;
			Ops.bSimplifyOutput = false;

			// Intersection (Debris): 원본 크기 DebrisToolMesh 사용
			if (UnionResult.DebrisSharedToolMesh.IsValid() && UnionResult.IslandContext.IsValid())
			{
				FDynamicMesh3 DebrisTool = *UnionResult.DebrisSharedToolMesh;
				FDynamicMesh3 Debris;

				UE_LOG(LogTemp, Warning, TEXT("[BooleanProcessor] Intersection START - WorkMesh Tris=%d, DebrisTool Tris=%d"),
					WorkMesh.TriangleCount(), DebrisTool.TriangleCount());

				bool bSuccessIntersection = ApplyMeshBooleanAsync(
					&WorkMesh,
					&DebrisTool,
					&Debris,
					EGeometryScriptBooleanOperation::Intersection,
					Ops);

				UE_LOG(LogTemp, Warning, TEXT("[BooleanProcessor] Intersection RESULT - bSuccess=%d, Debris Tris=%d"),
					bSuccessIntersection ? 1 : 0, Debris.TriangleCount());

				if (bSuccessIntersection && Debris.TriangleCount() > 0)
				{
					FScopeLock Lock(&UnionResult.IslandContext->MeshLock);
					// Initialize attributes
					if (UnionResult.IslandContext->AccumulatedDebrisMesh.TriangleCount() == 0)
					{
						UnionResult.IslandContext->AccumulatedDebrisMesh.EnableAttributes();
						UnionResult.IslandContext->AccumulatedDebrisMesh.Attributes()->EnableMaterialID();
						if (!UnionResult.IslandContext->AccumulatedDebrisMesh.HasTriangleGroups())
						{
							UnionResult.IslandContext->AccumulatedDebrisMesh.EnableTriangleGroups();
						}
					}

					FDynamicMeshEditor Editor(&UnionResult.IslandContext->AccumulatedDebrisMesh);
					FMeshIndexMappings Mappings;
					Editor.AppendMesh(&Debris, Mappings);
					bHasDebris = true;

					UE_LOG(LogTemp, Warning, TEXT("[BooleanProcessor] Accumulated Debris Tris=%d"),
						UnionResult.IslandContext->AccumulatedDebrisMesh.TriangleCount());
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[BooleanProcessor] Intersection SKIPPED - DebrisToolMesh=%d, IslandContext=%d"),
					UnionResult.DebrisSharedToolMesh.IsValid() ? 1 : 0, UnionResult.IslandContext.IsValid() ? 1 : 0);
			}

			// Subtract (구멍): 스케일된 SharedToolMesh 사용
			if (UnionResult.SharedToolMesh.IsValid())
			{
				FDynamicMesh3 LocalTool = *UnionResult.SharedToolMesh;
				bSuccess = ApplyMeshBooleanAsync(
					&WorkMesh,
					&LocalTool,
					&ResultMesh,
					EGeometryScriptBooleanOperation::Subtract,
					Ops);
			}
		} 
	}

	if (SlotSubtractWorkerCounts.IsValidIndex(SlotIndex))
	{
	 SlotSubtractWorkerCounts[SlotIndex]->fetch_sub(1);
	}
	
	// ===== 5. 应用结果 (GameThread) =====
	if (bSuccess || bHasDebris)
	{
		AsyncTask(ENamedThreads::GameThread,
		          [LifeTimeToken = LifeTime,
			          ChunkIndex,
			          SlotIndex,
			          ResultMesh = MoveTemp(ResultMesh),
			          Context = UnionResult.IslandContext,
			          Decals = MoveTemp(UnionResult.Decals),
			          UnionCount = UnionResult.UnionCount,
			          CompletionBatchIds = MoveTemp(UnionResult.CompletionBatchIds),
			          bSuccess]() mutable
		          {
			          if (!LifeTimeToken.IsValid() || !LifeTimeToken->bAlive.load())
			          {
				          return;
			          }

		          	TSharedPtr<FRealtimeBooleanProcessor, ESPMode::ThreadSafe> Processor = LifeTimeToken->Processor.Pin();
					  if (!Processor.IsValid())
					  {
						  return;
					  }

		          	URealtimeDestructibleMeshComponent* WeakOwner = Processor->OwnerComponent.Get();
			          if (!WeakOwner)
			          {
				          return;
			          }			         

			          double StartTime = FPlatformTime::Seconds();

			          // 应用网格。
			          if (bSuccess)
			          {
#if !UE_BUILD_SHIPPING
				          TRACE_CPUPROFILER_EVENT_SCOPE("SlotWorkerUnion_ApplyGT");
#endif
				          WeakOwner->ApplyBooleanOperationResult(MoveTemp(ResultMesh), ChunkIndex, true);
			          }

			          // 批处理完成跟踪：为所有BatchId通知完成
			          for (int32 BatchId : CompletionBatchIds)
			          {
				          WeakOwner->NotifyBooleanCompleted(BatchId);
			          }

			          double ExecutionDurationMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;

			          Processor->UpdateSimplifyInterval(ExecutionDurationMs, ChunkIndex);

			          // 生成碎片或应用于现有的DebrisActor（客户端提取）
			          if (Context.IsValid())
			          {
				          int32 RemainingBefore = Context->RemainingTaskCount.load();
				          UE_LOG(LogTemp, Warning, TEXT("[BooleanProcessor] Completion Callback - RemainingTasks=%d, AccumulatedTris=%d, TargetDebrisActor=%d"),
					          RemainingBefore, Context->AccumulatedDebrisMesh.TriangleCount(),
					          Context->TargetDebrisActor.IsValid() ? 1 : 0);

				          if (Context->RemainingTaskCount.fetch_sub(1) == 1)
				          {
					          if (Context->AccumulatedDebrisMesh.TriangleCount() > 0)
					          {
						          TArray<UMaterialInterface*> Materials;
						          if (auto ChunkMesh = WeakOwner->GetChunkMeshComponent(1))
						          {
							          for (int32 i = 0; i < ChunkMesh->GetNumMaterials(); i++)
							          {
								          Materials.Add(ChunkMesh->GetMaterial(i));
							          }
						          }

						          // 如果存在TargetDebrisActor，则将网格应用于现有Actor（客户端提取）
						          if (Context->TargetDebrisActor.IsValid())
						          {
							          UE_LOG(LogTemp, Warning, TEXT("[BooleanProcessor] Calling ApplyMeshToDebrisActor with %d triangles"), Context->AccumulatedDebrisMesh.TriangleCount());
							          WeakOwner->SpawnDebrisActor(MoveTemp(Context->AccumulatedDebrisMesh), Materials, Context->TargetDebrisActor.Get());
							          UE_LOG(LogTemp, Warning, TEXT("[BooleanProcessor] Applied mesh to existing DebrisActor"));
						          }
						          // 否则：生成新的DebrisActor（独立/侦听服务器）
						          else if (Context->Owner.IsValid())
						          {
							          Context->Owner->SpawnDebrisActor(MoveTemp(Context->AccumulatedDebrisMesh), Materials);
						          }
					          }

					          // IslandRemoval完成后清理小碎片
					          if (Context->DisconnectedCellsForCleanup.Num() > 0 && Context->Owner.IsValid())
					          {
						          Context->Owner->CleanupSmallFragments(Context->DisconnectedCellsForCleanup);
					          }

					          // 减少IslandRemoval计数器（用于与布尔批处理完成协调）
					          if (Context->Owner.IsValid())
					          {
						          Context->Owner->DecrementIslandRemovalCount();
					          }
				          }
			          }

			          // ===== 递减计数器（关机检查） =====
			          // if (Processor->SlotSubtractWorkerCounts.IsValidIndex(SlotIndex))
			          // {
				         //  Processor->SlotSubtractWorkerCounts[SlotIndex]->fetch_sub(1);
			          // }

			          // 更新计数器。
			          Processor->ChunkGenerations[ChunkIndex].fetch_add(1);
			          Processor->ChunkHoleCount[ChunkIndex] += UnionCount;

		          	WeakOwner->ClearChunkBusy(ChunkIndex);
					UE_LOG(LogTemp, Warning, TEXT("ClearChunkBusy: ChunkIndex=%d, QueueEmpty=%d"),
						ChunkIndex, Processor->SlotSubtractQueues[SlotIndex]->IsEmpty());
				    // 如果队列有工作，重新启动。 
					for (int32 i = 0; i < Processor->SlotSubtractQueues.Num(); i++)
					{
						if (!Processor->SlotSubtractQueues[i]->IsEmpty())
						{
							Processor->KickSubtractWorker(i);
						}
					}
				    // 下一次启动。
					Processor->KickProcessIfNeededPerChunk();
		          });
	}
	else
	{
		// 即使失败，如果队列有工作，也重新启动（GameThread）。
		AsyncTask(ENamedThreads::GameThread, [LifeTimeToken = LifeTime, SlotIndex, ChunkIndex = ChunkIndex, CompletionBatchIds = MoveTemp(UnionResult.CompletionBatchIds)]()
		{
			if (!LifeTimeToken.IsValid() || !LifeTimeToken->bAlive.load())
			{
				return;
			}

			TSharedPtr<FRealtimeBooleanProcessor, ESPMode::ThreadSafe> Processor = LifeTimeToken->Processor.Pin();
			if (!Processor.IsValid())
			{
				return;
			}

			URealtimeDestructibleMeshComponent* WeakOwner = Processor->OwnerComponent.Get();
			if (!WeakOwner)
			{
				return;
			}
			WeakOwner->ClearChunkBusy(ChunkIndex);

			// 即使失败也要跟踪批处理完成：为所有BatchId通知完成
			for (int32 BatchId : CompletionBatchIds)
			{
				WeakOwner->NotifyBooleanCompleted(BatchId);
			}

			// if (Processor->SlotSubtractWorkerCounts.IsValidIndex(SlotIndex))
			// {
			// 	Processor->SlotSubtractWorkerCounts[SlotIndex]->fetch_sub(1);
			// }

			for (int32 i = 0; i < Processor->SlotSubtractQueues.Num(); i++)
			{
				if (!Processor->SlotSubtractQueues[i]->IsEmpty())
				{
					Processor->KickSubtractWorker(i);
				}
			}
		});
	}
}

void FRealtimeBooleanProcessor::CleanupSlotMapping(int32 SlotIndex)
{
	// 确保联合队列也为空（两者都必须为空才能清理）。
	bool bUnionEmpty = SlotUnionQueues[SlotIndex]->IsEmpty();
	bool bSubtractEmpty = SlotSubtractQueues[SlotIndex]->IsEmpty();

	if (!bUnionEmpty || !bSubtractEmpty)
	{
		return;  // 仍有工作剩余。
	}
}

void FRealtimeBooleanProcessor::BooleanOpSync(FBulletHole&& Op)
{
	if (!OwnerComponent.IsValid())
	{
		return;
	}

	if (Op.ChunkIndex == INDEX_NONE)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE("BooleanSync");
	
	FDynamicMesh3 Result = {};
	FGeometryScriptMeshBooleanOptions Options = OwnerComponent->GetBooleanOptions();

	FDynamicMesh3 WorkMesh = *OwnerComponent->GetMesh();

	FDynamicMesh3 ToolMesh = MoveTemp(*Op.ToolMeshPtr.Get());
	MeshTransforms::ApplyTransform(ToolMesh, Op.ToolTransform, true);

	bool bBooleanSuccess = false;
	UE_LOG(LogTemp, Display, TEXT("BooleanSync"));
	{
		TRACE_CPUPROFILER_EVENT_SCOPE("BooleanSync_Subtact");
		bBooleanSuccess = ApplyMeshBooleanAsync(
			&WorkMesh,
			&ToolMesh,
			&Result,
			EGeometryScriptBooleanOperation::Subtract,
			Options);
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE("BooleanSync_Apply");
		if (bBooleanSuccess)
		{
			OwnerComponent->EditMesh([&](FDynamicMesh3 & InternalMesh)
			{
				InternalMesh = MoveTemp(Result);
			});

			OwnerComponent->ApplyCollisionUpdate(OwnerComponent.Get());
		}
	}
}

void FRealtimeBooleanProcessor::UpdateUnionSize(int32 ChunkIndex, double DurationMs)
{
	const int32 CurrentUnionCount = MaxUnionCount[ChunkIndex];
	int32 NextCount = CurrentUnionCount;
	
	if (DurationMs > FrameBudgetMs)
	{
		// 减少70%。
		NextCount = FMath::FloorToInt(CurrentUnionCount * 0.7f);

		/*
		 * 即使成本飙升，也至少钳位到1以保护帧。
		 */
		NextCount = FMath::Max(1, NextCount);
		
		UE_LOG(LogTemp, Display, TEXT("union size reduce %d to %d"), CurrentUnionCount, NextCount);
	}
	else if (DurationMs < (FrameBudgetMs * 0.6))
	{
		/*
		 * 1. 20似乎足够了。
		 * 2. 对每个网格进行性能分析是不现实的。
		 */
		NextCount = FMath::Min(CurrentUnionCount + 1, 20);

		UE_LOG(LogTemp, Display, TEXT("union size increase %d to %d"), CurrentUnionCount, NextCount);
	}

	if (NextCount != CurrentUnionCount)
	{
		MaxUnionCount[ChunkIndex] = NextCount;
	}
}

void FRealtimeBooleanProcessor::KickProcessIfNeededPerChunk()
{
	/*
	 * 按优先级构建TMap。
	 * 使用块地址作为键来收集每个块的操作。
	 */
	TMap<UDynamicMeshComponent*, FBulletHoleBatch> HighPriorityMap;
	TMap<UDynamicMeshComponent*, FBulletHoleBatch> NormalPriorityMap;

	/*
	 * TMap不保留顺序。
	 * 使用数组来保留顺序。
	 */
	TArray<UDynamicMeshComponent*> HighPriorityOrder;
	TArray<UDynamicMeshComponent*> NormalPriorityOrder;

	

	// 预分配一些内存。
	HighPriorityOrder.Reserve(100);
	NormalPriorityOrder.Reserve(100);

	auto GatherOps = [&](TQueue<FBulletHole, EQueueMode::Mpsc>& Queue,
	                     TMap<UDynamicMeshComponent*, FBulletHoleBatch>& OpMap,
	                     TArray<UDynamicMeshComponent*>& OrderArray, int& DebugCount)
	{
		// 用于超出联合限制的操作的临时溢出数组（用于重新入队）。
		TArray<FBulletHole> OverflowOps;
		OverflowOps.Reserve(50);
		
		FBulletHole Op;
		while (Queue.Dequeue(Op))
		{
			auto TargetMesh = Op.TargetMesh.Get(); 
			if (!TargetMesh)
			{
				continue;
			}

			const int32 ChunkIndex = OwnerComponent->GetChunkIndex(TargetMesh);
			if (ChunkIndex == INDEX_NONE)
			{
				continue;
			}
			
			const int32 ChunkUnionLimit = MaxUnionCount.IsValidIndex(ChunkIndex) ? MaxUnionCount[ChunkIndex] : 10;

			// 使用Find避免创建不必要的键。
			FBulletHoleBatch* Batch = OpMap.Find(TargetMesh);
			const int32 CurrentCount = Batch ? Batch->Num() : 0;

			// 如果超出联合限制，则存入溢出并重新入队。
			if (CurrentCount >= ChunkUnionLimit)
			{
				OverflowOps.Add(MoveTemp(Op));
			}
			else
			{
				if (!Batch)
				{
					// 记录顺序。
					OrderArray.Add(TargetMesh);

					// 创建映射条目。
					Batch = &OpMap.Add(TargetMesh);
					Batch->Reserve(ChunkUnionLimit);
				}
				Batch->Add(MoveTemp(Op));
				DebugCount--;
			}
		}

		for (FBulletHole& OverflowOp : OverflowOps)
		{
			Queue.Enqueue(MoveTemp(OverflowOp));
		}
	};

	{
#if !UE_BUILD_SHIPPING
		TRACE_CPUPROFILER_EVENT_SCOPE("GatherOps");
#endif
		GatherOps(HighPriorityQueue, HighPriorityMap, HighPriorityOrder, DebugHighQueueCount);
		GatherOps(NormalPriorityQueue, NormalPriorityMap, NormalPriorityOrder, DebugNormalQueueCount);
	}

	auto ProcessTargetMesh = [&](TMap<UDynamicMeshComponent*, FBulletHoleBatch>& OpMap,
	                             TQueue<FBulletHole, EQueueMode::Mpsc>& Queue,
	                             TArray<UDynamicMeshComponent*>& OrderArray, int32& DebugCount)
	{
		if (OpMap.IsEmpty() || OrderArray.IsEmpty())
		{
			return;
		}

		for (auto TargetMesh : OrderArray)
		{
			const int32 ChunkIndex = OwnerComponent->GetChunkIndex(TargetMesh);

			if (ChunkIndex == INDEX_NONE)
			{
				continue;
			}

			if (FBulletHoleBatch* Batch = OpMap.Find(TargetMesh))
			{
				Batch->ChunkIndex = ChunkIndex;
				UE_LOG(LogTemp, Display, TEXT("ToolMeshTri/lamda %d/ %d"), Batch->Num(), Batch->ToolMeshPtrs[0].Get()->TriangleCount());
				 if (bEnableMultiWorkers)
				 {
				 	// 决定此块的插槽。
				 	int32 TargetSlot = FindLeastBusySlot();
				 	
				 	// 入队到联合队列。
				  	SlotUnionQueues[TargetSlot]->Enqueue(MoveTemp(*Batch));
				 	// 唤醒联合工作线程。
				 	KickUnionWorker(TargetSlot);
				 }
				 else
				 {
				 	if (!OwnerComponent->CheckAndSetChunkBusy(ChunkIndex))
				 	{
				 		const int32 Gen = ChunkGenerations[ChunkIndex];
				 		StartBooleanWorkerAsyncForChunk(MoveTemp(*Batch), Gen);
				 	}
				 	else
				 	{
				 		/*
				 			* 添加逻辑以重新排队繁忙块的批次。
				 			*/
				 		EnqueueRetryOps(Queue, MoveTemp(*Batch), TargetMesh, ChunkIndex, DebugCount);
				 	}
				 }
			}
		}
	};

	ProcessTargetMesh(HighPriorityMap, HighPriorityQueue, HighPriorityOrder, DebugHighQueueCount);
	ProcessTargetMesh(NormalPriorityMap, NormalPriorityQueue, NormalPriorityOrder, DebugNormalQueueCount);
}

void FRealtimeBooleanProcessor::StartBooleanWorkerAsyncForChunk(FBulletHoleBatch&& InBatch, int32 Gen)
{
	if (InBatch.Num() == 0 || !OwnerComponent.IsValid())
	{
		return;
	}

	FGeometryScriptMeshBooleanOptions Options = OwnerComponent->GetBooleanOptions();
	UE::Tasks::Launch(
		UE_SOURCE_LOCATION,
		[OwnerComponent = OwnerComponent, LifeTimeToken = LifeTime,
		Batch = MoveTemp(InBatch), Options, Gen]() mutable
		{
			// 安全地清除繁忙位。
			auto SafeClearBusyBit = [&]()
				{
					if (OwnerComponent.IsValid())
					{
						int32 IndexToClear = Batch.ChunkIndex;
						AsyncTask(ENamedThreads::GameThread, [OwnerComponent, IndexToClear]()
							{
								if (OwnerComponent.IsValid())
								{
									OwnerComponent->ClearChunkBusy(IndexToClear);
								}
							});
					}
				};

			if (!OwnerComponent.IsValid())
			{
				return;
			}

#if !UE_BUILD_SHIPPING
			TRACE_CPUPROFILER_EVENT_SCOPE("ChunkBooleanAsync");
#endif
			TSharedPtr<FRealtimeBooleanProcessor, ESPMode::ThreadSafe> Processor = LifeTimeToken->Processor.Pin();
			if (!Processor.IsValid())
			{
				SafeClearBusyBit();
				return;
			}

			const int32 BatchCount = Batch.Num();
			if (BatchCount <= 0)
			{
				SafeClearBusyBit();
				return;
			}

			const int32 ChunkIndex = Batch.ChunkIndex;
			// 复制目标网格。
			FDynamicMesh3 WorkMesh;
			if (!OwnerComponent->GetChunkMesh(WorkMesh, ChunkIndex))
			{
				SafeClearBusyBit();
				return;
			}

			using namespace UE::Geometry;

			int32 AppliedCount = 0;
			TArray<TWeakObjectPtr<UDecalComponent>> DecalsToRemove;
			DecalsToRemove.Reserve(BatchCount);
			// 批处理完成跟踪ID数组（在移动其他数据之前首先提取）
			TArray<int32> CompletionBatchIds = MoveTemp(Batch.CompletionBatchIds);
			TArray<TWeakObjectPtr<UDecalComponent>> TemporaryDecals = MoveTemp(Batch.TemporaryDecals);
			TArray<FTransform> Transforms = MoveTemp(Batch.ToolTransforms);
			TArray<TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe>> ToolMeshPtrs = MoveTemp(Batch.ToolMeshPtrs);

			int32 UnionCount = 0;
			bool bIsFirst = true;
			bool bCombinedValid = false;
			FDynamicMesh3 CombinedToolMesh;
			{
#if !UE_BUILD_SHIPPING
				TRACE_CPUPROFILER_EVENT_SCOPE("ChunkBooleanAsync_Union");
#endif
				for (int32 i = 0; i < BatchCount; i++)
				{

					if (!ToolMeshPtrs[i].IsValid())
					{
						SafeClearBusyBit();
						return;
					}

					FTransform ToolTransform = MoveTemp(Transforms[i]);
					TWeakObjectPtr<UDecalComponent> TemporaryDecal = MoveTemp(TemporaryDecals[i]);

					FDynamicMesh3 CurrentTool = *(ToolMeshPtrs[i]);
					MeshTransforms::ApplyTransform(CurrentTool, (FTransformSRT3d)ToolTransform, true);

					if (TemporaryDecal.IsValid())
					{
						DecalsToRemove.Add(MoveTemp(TemporaryDecal));
					}

					if (bIsFirst)
					{
						bIsFirst = false;
						CombinedToolMesh = CurrentTool;
						bCombinedValid = true;
						UnionCount++;
					}
					else
					{
						FDynamicMesh3 UnionResult;
						FMeshBoolean MeshUnion(&CombinedToolMesh, FTransform::Identity,
							&CurrentTool, FTransform::Identity,
							&UnionResult, FMeshBoolean::EBooleanOp::Union);
						if (MeshUnion.Compute())
						{
							CombinedToolMesh = MoveTemp(UnionResult);
							UnionCount++;
						}
					}
				}
			}
						
			bool bSubtractSuccess = false;
			if (bCombinedValid && CombinedToolMesh.TriangleCount() > 0)
			{
				const double SubtractStartTime = FPlatformTime::Seconds();

				FDynamicMesh3 ResultMesh;
				{
#if !UE_BUILD_SHIPPING
					TRACE_CPUPROFILER_EVENT_SCOPE("ChunkBooleanAsync_Subtract");
#endif
					if (CombinedToolMesh.TriangleCount() > 0)
					{
						FAxisAlignedBox3d ToolBounds = CombinedToolMesh.GetBounds();
						FAxisAlignedBox3d TargetBounds = WorkMesh.GetBounds();

						// 目标网格信息（注释掉以避免日志垃圾信息）。
						// UE_LOG(LogTemp, Warning, TEXT("[Boolean Debug] CombinedToolMesh Center: %s, Size: %s"),
						// 	*FVector(ToolBounds.Center()).ToString(),
						// 	*FVector(ToolBounds.Extents()).ToString());
						//
						// UE_LOG(LogTemp, Warning, TEXT("[Boolean Debug] WorkMesh(Target) Center: %s, Size: %s"),
						// 	*FVector(TargetBounds.Center()).ToString(),
						// 	*FVector(TargetBounds.Extents()).ToString());
					}

					bSubtractSuccess = ApplyMeshBooleanAsync(&WorkMesh, &CombinedToolMesh, &ResultMesh,
					                                         EGeometryScriptBooleanOperation::Subtract, Options);
				}
				// 重新检查处理器有效性（可能在异步期间被销毁）。
				if (!Processor.IsValid())
				{
					SafeClearBusyBit();
					return;
				}
				++Processor->ChunkGenerations[ChunkIndex];

				const double CurrentSubDurationMs = (FPlatformTime::Seconds() - SubtractStartTime) * 1000.0;

				if (bSubtractSuccess)
				{
					// 对合并的子弹数应用布尔结果。
					AppliedCount = UnionCount;
					WorkMesh = MoveTemp(ResultMesh);

					Processor->AccumulateSubtractDuration(ChunkIndex, CurrentSubDurationMs);

					{
						// 网格简化。
#if !UE_BUILD_SHIPPING
						TRACE_CPUPROFILER_EVENT_SCOPE("ChunkBooleanAsync_Simplify");
#endif
						bool bEnableDetailMode = OwnerComponent->IsHighDetailMode();
						bool bIsSimplified = Processor->TrySimplify(WorkMesh, ChunkIndex, UnionCount, bEnableDetailMode);
				}

					
					Processor->UpdateUnionSize(ChunkIndex, CurrentSubDurationMs);
				}
				else
				{ 
					// 失败时重置累积。
					FChunkState& State = Processor->ChunkStates.GetState(ChunkIndex);
					State.SubtractDurationAccum = 0;
					State.DurationAccumCount = 0;
				}
			}

			AsyncTask(ENamedThreads::GameThread,
				[OwnerComponent, LifeTimeToken, Gen, ChunkIndex, Result = MoveTemp(WorkMesh), AppliedCount, DecalsToRemove = MoveTemp(DecalsToRemove), CompletionBatchIds = MoveTemp(CompletionBatchIds)]() mutable
				{
					if (!OwnerComponent.IsValid())
					{
						return;
					}
					OwnerComponent->ClearChunkBusy(ChunkIndex);

					if (!LifeTimeToken.IsValid() || !LifeTimeToken->bAlive.load())
					{
						return;
					}

					TSharedPtr<FRealtimeBooleanProcessor, ESPMode::ThreadSafe> Processor = LifeTimeToken->Processor.Pin();
					if (!Processor.IsValid())
					{
						return;
					}

					if (OwnerComponent->GetBooleanProcessor() != Processor.Get())
					{
						return;
					}

#if !UE_BUILD_SHIPPING
					TRACE_CPUPROFILER_EVENT_SCOPE("ChunkBooleanAsync_ApplyGT");
#endif

					if (AppliedCount > 0)
					{
						const double SetMeshStartTime = FPlatformTime::Seconds();
						{
#if !UE_BUILD_SHIPPING
							TRACE_CPUPROFILER_EVENT_SCOPE("ChunkBooleanAsync_SetMesh");
#endif
							OwnerComponent->ApplyBooleanOperationResult(MoveTemp(Result), ChunkIndex, false);
						}
						const double CurrentSetMeshAvgCost = (FPlatformTime::Seconds()- SetMeshStartTime) * 1000.0;

						Processor->UpdateSimplifyInterval(CurrentSetMeshAvgCost, ChunkIndex);

						for (const TWeakObjectPtr<UDecalComponent>& Decal : DecalsToRemove)
						{
							if (Decal.IsValid())
							{
								//Decal->DestroyComponent();
							}
						}
					}

					// 批处理完成跟踪：为所有BatchId通知完成
					for (int32 BatchId : CompletionBatchIds)
					{
						OwnerComponent->NotifyBooleanCompleted(BatchId);
					}

					Processor->ChunkHoleCount[ChunkIndex] += AppliedCount;

					Processor->KickProcessIfNeededPerChunk();
				});
		});
}

void FRealtimeBooleanProcessor::CancelAllOperations()
{
	SetMeshAvgCost.Reset();

	InitInterval = 0;

	FBulletHole Temp;
	while (HighPriorityQueue.Dequeue(Temp)) {}
	while (NormalPriorityQueue.Dequeue(Temp)) {}

	DebugHighQueueCount = 0;
	DebugNormalQueueCount = 0;

	// 重置孔计数和缓存。
	ChunkStates.Reset();
	ChunkHoleCount.Init(OwnerComponent->GetChunkNum(), 0);
}

void FRealtimeBooleanProcessor::AccumulateSubtractDuration(int32 ChunkIndex, double CurrentSubDuration)
{
	FChunkState& State = ChunkStates.GetState(ChunkIndex);
	// 如果超过阈值，则累积时间。
	if (CurrentSubDuration >= SubDurationHighThreshold) 
	{
		State.SubtractDurationAccum += CurrentSubDuration;
		State.DurationAccumCount++;
		UE_LOG(LogTemp, Display, TEXT("Accumulate Duration %d"), State.DurationAccumCount);
	}
	// 如果之前已累积但此帧低于阈值，则重置。
	else if (CurrentSubDuration < SubDurationHighThreshold && State.DurationAccumCount > 0)
	{
		State.SubtractDurationAccum = 0;
		State.DurationAccumCount = 0;
		UE_LOG(LogTemp, Display, TEXT("Accumulate Reset"));
	}
}

void FRealtimeBooleanProcessor::UpdateSimplifyInterval(double CurrentSetMeshAvgCost, int32 ChunkIndex)
{
	// 范围检查：异步操作期间可能会删除块
	if (ChunkIndex < 0 || ChunkIndex >= SetMeshAvgCost.Num())
	{
		return;
	}

	if (FMath::IsNearlyZero(SetMeshAvgCost[ChunkIndex]))
	{
		SetMeshAvgCost[ChunkIndex] = CurrentSetMeshAvgCost;
		return;
	}

	const double OldAvgCost = SetMeshAvgCost[ChunkIndex];

	// 计算指数移动平均（EMA，类似于线性插值）。
	const double NewAvgCost = FMath::Lerp(SetMeshAvgCost[ChunkIndex], CurrentSetMeshAvgCost, 0.1);
	SetMeshAvgCost[ChunkIndex] = NewAvgCost;

	// 减少率：（旧 - 新）/ 旧。
	const double ReductionRate = (OldAvgCost - NewAvgCost) / OldAvgCost;

	/*
	 * 调整常数。
	 */
	 // 增加阈值；高于此值则减少间隔。
	constexpr double PanicThreshold = 0.1;
	// 稳定阈值；在此值或以上则增加间隔。
	constexpr double StableThreshold = 0.0;

	/*
	 * AIMD（加性增加，乘性减少）用于TCP拥塞控制。
	 * 缓慢增加并快速减少以应对成本峰值。
	 */

	 // 当成本增加>=10%时减少间隔。
	if (-ReductionRate > PanicThreshold)
	{
		UE_LOG(LogTemp, Display, TEXT("Interval decrease %d to %lld"), MaxInterval[ChunkIndex], FMath::FloorToInt(MaxInterval[ChunkIndex] * 0.7));
		// 将下限钳位到15。
		MaxInterval[ChunkIndex] = FMath::Max(15, FMath::FloorToInt(MaxInterval[ChunkIndex] * 0.7));
	}
	// 如果成本降低或持平，则缓慢增加。
	else if (ReductionRate >= StableThreshold)
	{
		UE_LOG(LogTemp, Display, TEXT("Interval increase %d to %d"), MaxInterval[ChunkIndex], MaxInterval[ChunkIndex] + 1);
		MaxInterval[ChunkIndex] = FMath::Min(InitInterval * 2, MaxInterval[ChunkIndex] + 1);
	}
	// 观察并保持0-10%的增长。
	// 保留else分支用于日志记录。
	else
	{
		UE_LOG(LogTemp, Display, TEXT("Interval hold"));
	}
}

bool FRealtimeBooleanProcessor::TrySimplify(UE::Geometry::FDynamicMesh3& WorkMesh, int32 ChunkIndex, int32 UnionCount, bool bEnableDetail)
{
	if (!FRDMCVarHelper::EnableSimplify())
	{
		UE_LOG(LogTemp, Display, TEXT("Simplify Off"));
		return false;
	}
	if (!ChunkStates.States.IsValidIndex(ChunkIndex))
	{
		return false;
	}
	
	FChunkState& State = ChunkStates.GetState(ChunkIndex);
	State.Interval += UnionCount;
	
	bool bShouldSimplify = false;
	const int32 TriCount = WorkMesh.TriangleCount();

	if ((TriCount > State.LastSimplifyTriCount * 1.2f &&
		State.LastSimplifyTriCount > 1000) ||
		TriCount - State.LastSimplifyTriCount > 1000)
	{
		UE_LOG(LogTemp, Display, TEXT("Simplify/TriCount"));
		bShouldSimplify = true;
	}
	// 2次累积后，如果平均值超过阈值，则简化。
	else if (State.DurationAccumCount >= 2 &&
		State.SubtractDurationAccum / State.DurationAccumCount >= SubDurationHighThreshold)
	{
		UE_LOG(LogTemp, Display, TEXT("Simplify/Duration"));
		bShouldSimplify = true;
	}
	// 当间隔达到最大值时简化。
	else if (State.Interval >= MaxInterval[ChunkIndex])
	{
		UE_LOG(LogTemp, Display, TEXT("Simplify/Interval"));
		bShouldSimplify = true;
	}

	if (bShouldSimplify)
	{
		State.Reset();	
		FGeometryScriptPlanarSimplifyOptions SimplifyOptions;
		// SimplifyOptions.bAutoCompact = true;
		SimplifyOptions.bAutoCompact = false;
		SimplifyOptions.AngleThreshold = AngleThreshold;
		ApplySimplifyToPlanarAsync(&WorkMesh, SimplifyOptions, bEnableDetail);
		/*
		 * 截至12/26，运行时不在GT上访问LastSimplifyTriCount。
		 */
		State.LastSimplifyTriCount = WorkMesh.TriangleCount();
	}

	return bShouldSimplify;
}

void FRealtimeBooleanProcessor::EnqueueRetryOps(TQueue<FBulletHole, EQueueMode::Mpsc>& Queue, FBulletHoleBatch&& InBatch,
	UDynamicMeshComponent* TargetMesh, int32 ChunkIndex, int32& DebugCount)
{
	int32 BatchCount = InBatch.Num();
	if (BatchCount == 0)
	{
		return;
	}

	FBulletHole Op = {};
	for (int32 i = 0; i < BatchCount; i++)
	{
		if (InBatch.Get(Op, i))
		{
			Op.ChunkIndex = ChunkIndex;
			Op.TargetMesh = TargetMesh;
			Queue.Enqueue(Op);
			DebugCount++;
		}
		Op.Reset();
	}
}

int32& FRealtimeBooleanProcessor::GetChunkInterval(int32 ChunkIndex)
{
	/*
	 * 调用前验证索引。
	 */
	return ChunkStates.GetState(ChunkIndex).Interval;
}

int32 FRealtimeBooleanProcessor::GetChunkHoleCount(const UPrimitiveComponent* ChunkComponent) const
{
	if (!ChunkComponent)
	{
		return INDEX_NONE;
	}
	
	int32 ChunkIndex = OwnerComponent->GetChunkIndex(ChunkComponent);

	return GetChunkHoleCount(ChunkIndex);
}

bool FRealtimeBooleanProcessor::ApplyMeshBooleanAsync(const UE::Geometry::FDynamicMesh3* TargetMesh,
                                                      const UE::Geometry::FDynamicMesh3* ToolMesh,
                                                      UE::Geometry::FDynamicMesh3* OutputMesh,
                                                      const EGeometryScriptBooleanOperation Operation,
                                                      const FGeometryScriptMeshBooleanOptions Options,
                                                      const FTransform& TargetTransform,
                                                      const FTransform& ToolTransform)
{
	check(TargetMesh != nullptr && ToolMesh != nullptr && OutputMesh != nullptr);

	// 空网格检查以避免AABB树构建崩溃。
	if (TargetMesh->TriangleCount() == 0 || ToolMesh->TriangleCount() == 0)
	{
		return false;
	}

	using namespace UE::Geometry;

	// 如果需要，扩展到其他操作。
	FMeshBoolean::EBooleanOp Op = FMeshBoolean::EBooleanOp::Difference;
	switch (Operation)
	{
	case EGeometryScriptBooleanOperation::Subtract:
		Op = FMeshBoolean::EBooleanOp::Difference;
		break;
	case EGeometryScriptBooleanOperation::Intersection:
		Op = FMeshBoolean::EBooleanOp::Intersect;
		break;
	case EGeometryScriptBooleanOperation::Union:
		Op = FMeshBoolean::EBooleanOp::Union;
		break;
	default:
		Op = FMeshBoolean::EBooleanOp::Difference;
		break;
	}

	const int32 MaxAttempts = 2;
	for (int32 Attempt = 0; Attempt < MaxAttempts; Attempt++)
	{
		FTransform CurrentToolTransform = ToolTransform;

		// 第一次失败时，抖动位置/旋转并重试。
		if (Attempt > 0)
		{
			// 1.5mm
			const float JitterAmount = 0.015f;
			// 0.1 deg
			const float JitterAngle = 0.1f;

			FVector RandomOffset(
				FMath::FRandRange(-JitterAmount, JitterAmount),
				FMath::FRandRange(-JitterAmount, JitterAmount),
				FMath::FRandRange(-JitterAmount, JitterAmount));

			FQuat RandomRot(FVector::UpVector, FMath::DegreesToRadians(
				                FMath::FRandRange(-JitterAngle, JitterAngle)));

			CurrentToolTransform.AddToTranslation(RandomOffset);
			CurrentToolTransform.SetRotation(CurrentToolTransform.GetRotation() * RandomRot);

			UE_LOG(LogTemp, Log, TEXT("[Boolean] Attempt %d: Retrying with Jitter"), Attempt);
		}

		const int32 InternalMaterialID = 1;

		// 网格操作。
		FMeshBoolean MeshBoolean(
			TargetMesh, (FTransformSRT3d)TargetTransform,
			ToolMesh, (FTransformSRT3d)CurrentToolTransform,
			OutputMesh, Op);

		MeshBoolean.bPutResultInInputSpace = true;
		MeshBoolean.bSimplifyAlongNewEdges = Options.bSimplifyOutput;
		MeshBoolean.bWeldSharedEdges = false;

		bool bSuccess = MeshBoolean.Compute();
		if (bSuccess)
		{
			// 在OutputMesh上启用属性/材质ID。
			if (!OutputMesh->HasAttributes())
			{
				OutputMesh->EnableAttributes();
			}
			if (!OutputMesh->Attributes()->HasMaterialID())
			{
				OutputMesh->Attributes()->EnableMaterialID();
			}

			// 如果缺少，则启用PolyGroup层（保留布尔运算中的组）。
			if (!OutputMesh->HasTriangleGroups())
			{
				OutputMesh->EnableTriangleGroups();
			}

			FDynamicMeshMaterialAttribute* MaterialIDAttr = OutputMesh->Attributes()->GetMaterialID();

			// 使用PolyGroup ID分配。
			int32 ToolGroupID = 1; // 必须与ToolMesh上设置的ID匹配。
			for (int32 TriID : OutputMesh->TriangleIndicesItr())
			{
				if (OutputMesh->GetTriangleGroup(TriID) == ToolGroupID)
				{
					MaterialIDAttr->SetValue(TriID, InternalMaterialID);
				}
			}

			/*
				 * 焊接开放边。
				*/
			if (MeshBoolean.CreatedBoundaryEdges.Num() > 0)
			{
				// 选择开放边。
				TSet<int32> EdgeSet(MeshBoolean.CreatedBoundaryEdges);

				FMergeCoincidentMeshEdges Welder(OutputMesh);
				// 要焊接的边。
				Welder.EdgesToMerge = &EdgeSet;
				/*
				 * 仅为每条边合并1:1对应的边。
				 * 如果边A有候选B和C，则形成对（A,B）或（A,C）。
				 * 跳过合并目标不明确的模糊情况。
				 */
				Welder.OnlyUniquePairs = true;
				// 禁用属性的焊接。
				Welder.bWeldAttrsOnMergedEdges = false;

				// 匹配顶点的容差。
				Welder.MergeVertexTolerance = 0.001;
				// 合并候选的搜索容差。
				Welder.MergeSearchTolerance = 0.001;

				Welder.Apply();
			}

			return true;
		}
		// 失败时清除并重试。
		OutputMesh->Clear();
	}

	UE_LOG(LogTemp, Warning, TEXT("[Boolean] All attempts failed."));
	return false;
}

void FRealtimeBooleanProcessor::ApplySimplifyToPlanarAsync(UE::Geometry::FDynamicMesh3* TargetMesh, FGeometryScriptPlanarSimplifyOptions Options, bool bEnableDetail)
{
	if (!TargetMesh)
	{
		return;
	}
	using namespace UE::Geometry;

	FQEMSimplification Simplifier(TargetMesh);	

	if (bEnableDetail)
	{
		if (!TargetMesh->HasAttributes())
		{
			TargetMesh->EnableAttributes();
		}

		if (FRDMCVarHelper::GetSimplifyMode() == 0)
		{
			UE_LOG(LogTemp, Display, TEXT("HighDetail"));
			MeshClusterSimplify::FSimplifyOptions SimplifyOptions;
			SimplifyOptions.TargetEdgeLength = 1.0;
			SimplifyOptions.PreserveEdges.PolyGroup = MeshClusterSimplify::FSimplifyOptions::EConstraintLevel::Constrained;
			SimplifyOptions.PreserveEdges.UVSeam = MeshClusterSimplify::FSimplifyOptions::EConstraintLevel::Constrained;
			SimplifyOptions.PreserveEdges.Material = MeshClusterSimplify::FSimplifyOptions::EConstraintLevel::Constrained;
			SimplifyOptions.bTransferAttributes = true;
			SimplifyOptions.bTransferGroups = true;

			FDynamicMesh3 SimplifiedMesh;
			if (MeshClusterSimplify::Simplify(*TargetMesh, SimplifiedMesh, SimplifyOptions))
			{
				*TargetMesh = MoveTemp(SimplifiedMesh);
			}
		}

		if (FRDMCVarHelper::GetSimplifyMode() == 1)
		{
			FDynamicMeshAttributeSet* Attributes = TargetMesh->Attributes();
			const bool bHasMaterialID = Attributes && Attributes->HasMaterialID();

			const FDynamicMeshMaterialAttribute* MaterialID = bHasMaterialID ? Attributes->GetMaterialID() : nullptr;
			const FDynamicMeshUVOverlay* PrimaryUV = (Attributes) ? Attributes->PrimaryUV() : nullptr;

			// 用于保护表面（MaterialID = 0）上的接缝/材质边界的标志。
			// 当没有材质ID存在时禁用（无法检测表面）。
			const bool bSurfaceOnlyProtection = bHasMaterialID;
		
			const int32 SurfacematerialID = 0;

			// 用于检查边是否属于表面（MatID = 0）三角形的Lambda。
			// 假设仅在bSurfaceOnlyProtection为true时调用。
			auto EdgeTouchesSurface = [&](int32 EdgeID) -> bool
			{
				const FIndex2i EdgeTris = TargetMesh->GetEdgeT(EdgeID);
				if (EdgeTris.A >= 0 && MaterialID->GetValue(EdgeTris.A) == SurfacematerialID)
				{
					return true;
				}

				if (EdgeTris.B >= 0 && MaterialID->GetValue(EdgeTris.B) == SurfacematerialID)
				{
					return true;
				}

				return false;
			};

			/*
			 * 约束设置：在表面（MatID = 0）上保护以下边。
			 * - UV接缝边（主UV接缝）
			 * - MaterialID边界边
			 * - 边界边（开放边）受全局Simplifier.MeshBoundaryConstraint标志保护
			 * - 内部（MatID = 1）允许接缝塌陷
			 */
			TOptional<FMeshConstraints> ExternalConstraints;
			ExternalConstraints.Emplace();
			FMeshConstraints& Constraints = ExternalConstraints.GetValue();
			const FEdgeConstraint NoCollapseEdge(EEdgeRefineFlags::NoCollapse);

			// 表面保护逻辑仅在存在MatID时运行。
			if (bSurfaceOnlyProtection)
			{
				// Iterate all edges and select protected ones.
				for (int32 EdgeID : TargetMesh->EdgeIndicesItr())
				{
					// Skip boundary edges; protected by global flag.
					if (TargetMesh->IsBoundaryEdge(EdgeID))
					{
						continue;
					}

					// Skip non-surface edges.
					if (!EdgeTouchesSurface(EdgeID))
					{
						continue;
					}

					// Protect UV seam edges.
					if (PrimaryUV && PrimaryUV->IsSeamEdge(EdgeID))
					{
						Constraints.SetOrUpdateEdgeConstraint(EdgeID, NoCollapseEdge);
						continue;
					}

					// Protect edges with different materials (material boundary).
					const FIndex2i EdgeTris = TargetMesh->GetEdgeT(EdgeID);
					if (EdgeTris.A >= 0 && EdgeTris.B >= 0)
					{
						const int32 MatA = MaterialID->GetValue(EdgeTris.A);
						const int32 MatB = MaterialID->GetValue(EdgeTris.B);
						if (MatA != MatB)
						{
							Constraints.SetOrUpdateEdgeConstraint(EdgeID, NoCollapseEdge);
						}
					}
				}
			}

			// Protect boundary edges.
			// Boundary edge: edge with only one adjacent triangle.
			Simplifier.MeshBoundaryConstraint = EEdgeRefineFlags::NoCollapse;

			// Allow global seam collapse for interior simplification; surface seams are constrained externally.
			Simplifier.bAllowSeamCollapse = true;

			/*	 
			 * Simplify merges two vertices and must choose a new vertex position.
			 * MinimalExistingVertexError picks an existing vertex as the merged position.
			 * Other modes create new positions; repeated use can drift away from the surface.
			 * This drift is called positional drift.
			 */
			Simplifier.CollapseMode = FQEMSimplification::ESimplificationCollapseModes::MinimalQuadricPositionError;

			Simplifier.SetExternalConstraints(MoveTemp(ExternalConstraints));

			Simplifier.SimplifyToMinimalPlanar(FMath::Max(0.001, Options.AngleThreshold));
		}
	}
	else
	{
		Simplifier.CollapseMode = FQEMSimplification::ESimplificationCollapseModes::AverageVertexPosition;

		Simplifier.SimplifyToMinimalPlanar(FMath::Max(0.001, Options.AngleThreshold));
	}

	if (Options.bAutoCompact)
	{
		TargetMesh->CompactInPlace();
	}
}

void FRealtimeBooleanProcessor::ApplyUniformRemesh(FDynamicMesh3* TargetMesh, double TargetEdgeLength, int32 NumPasses)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Debris_ApplyUniformRemesh)

	if (!TargetMesh || TargetMesh->TriangleCount() == 0)
	{
		return;
	}

	FRemesher Remesher(TargetMesh);
	Remesher.SetTargetEdgeLength(TargetEdgeLength);

	// Constrain boundary edges to preserve mesh silhouette.
	TOptional<FMeshConstraints> ExternalConstraints;
	ExternalConstraints.Emplace();

	FMeshConstraintsUtil::ConstrainAllBoundariesAndSeams(
		ExternalConstraints.GetValue(),
		*TargetMesh,
		EEdgeRefineFlags::FullyConstrained,   // MeshBoundaryConstraint
		EEdgeRefineFlags::NoConstraint,        // GroupBoundaryConstraint
		EEdgeRefineFlags::NoConstraint,        // MaterialBoundaryConstraint
		true,   // bAllowSeamSplits
		true,   // bAllowSeamSmoothing
		false,  // bAllowSeamCollapse
		true    // bParallel
	);

	Remesher.SetExternalConstraints(MoveTemp(ExternalConstraints));

	for (int32 i = 0; i < NumPasses; ++i)
	{
		Remesher.BasicRemeshPass();
	}

	TargetMesh->CompactInPlace();
}