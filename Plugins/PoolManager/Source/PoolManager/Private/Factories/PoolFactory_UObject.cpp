// Copyright (c) Yevhenii Selivanov

#include "Factories/PoolFactory_UObject.h"

// Pool Manager
#include "Data/PoolManagerSettings.h"
#include "Data/PoolObjectData.h"
#include "Data/PoolObjectState.h"
#include "Data/TakeFromPoolPayload.h"
#include "PoolObjectCallback.h"

// UE
#include "Engine/World.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PoolFactory_UObject)

/*********************************************************************************************
 * 创建
 ********************************************************************************************* */

// 将对象生成请求加入队列
void UPoolFactory_UObject::RequestSpawn_Implementation(const FSpawnRequest& Request)
{
	if (!ensureMsgf(Request.IsValid(), TEXT("ASSERT: [%i] %hs:\n'Request' is not valid and can't be processed!"), __LINE__, __FUNCTION__))
	{
		return;
	}

	// 按优先级查找正确插入位置的 Lambda
	auto FindInsertionIndex = [&](ESpawnRequestPriority Priority)
	{
		int32 InsertIdx = 0;
		for (int32 Index = 0; Index < SpawnQueue.Num(); ++Index)
		{
			if (SpawnQueue[Index].Priority < Priority)
			{
				break;
			}
			InsertIdx = Index + 1;
		}
		return InsertIdx;
	};

	// 按优先级插入请求
	switch (Request.Priority)
	{
		case ESpawnRequestPriority::Critical:
		{
			// Critical 优先级立即处理
			ProcessRequestNow(Request);
			// Critical 请求不入队，直接返回
			return;
		}

		case ESpawnRequestPriority::High: // 继续向下执行
		case ESpawnRequestPriority::Medium:
		{
			// 用上面的 Lambda 按优先级找到正确的插入位置
			const int32 InsertIdx = FindInsertionIndex(Request.Priority);
			SpawnQueue.Insert(Request, InsertIdx);
		}
		break;

		case ESpawnRequestPriority::Normal:
			// Normal 优先级追加到队列末尾
			SpawnQueue.Emplace(Request);
			break;

		default:
			ensureAlwaysMsgf(false, TEXT("ASSERT: [%i] %hs:\n'Priority' is not valid: %d"), __LINE__, __FUNCTION__, static_cast<int32>(Request.Priority));
	}

	// 若这是队列中的第一个对象，则安排 OnNextTickProcessSpawn 在下一帧调用
	// 在其他线程创建 UObject 并非线程安全，且会引发垃圾回收问题，
	// 因此仍在游戏线程创建，但延迟到下一帧以避免卡顿
	if (SpawnQueue.Num() == 1)
	{
		const UWorld* World = GetWorld();
		checkf(World, TEXT("ERROR: [%i] %hs:\n'World' is null!"), __LINE__, __FUNCTION__);

		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::OnNextTickProcessSpawn);
	}
}

// 从队列中取出并移除第一个生成请求
bool UPoolFactory_UObject::DequeueSpawnRequest(FSpawnRequest& OutRequest)
{
	bool bResult = false;
	if (SpawnQueue.IsValidIndex(0))
	{
		// 复制并移除队首请求（不用 Swap 以保持顺序）
		OutRequest = SpawnQueue[0];
		SpawnQueue.RemoveAt(0);

		bResult = OutRequest.IsValid();
	}
	return ensureAlwaysMsgf(bResult, TEXT("ASSERT: [%i] %hs:\nFailed to dequeue the spawn request, handle is '%s'!"), __LINE__, __FUNCTION__, *OutRequest.Handle.GetHash().ToString());
}

// 以给定请求调用 SpawnNow 并处理相应回调
void UPoolFactory_UObject::ProcessRequestNow(const FSpawnRequest& Request)
{
	UObject* CreatedObject = SpawnNow(Request);
	checkf(CreatedObject, TEXT("ERROR: [%i] %hs:\n'CreatedObject' failed to spawn!"), __LINE__, __FUNCTION__);

	FPoolObjectData ObjectData;
	ObjectData.bIsActive = true;
	ObjectData.PoolObject = CreatedObject;
	ObjectData.Handle = Request.Handle;

	OnPreRegistered(Request, ObjectData);
	OnPostSpawned(Request, ObjectData);
}

// 另一种出队方式：按句柄从队列中取出并移除指定的生成请求
bool UPoolFactory_UObject::DequeueSpawnRequestByHandle(const FPoolObjectHandle& Handle, FSpawnRequest& OutRequest)
{
	if (!ensureMsgf(Handle.IsValid(), TEXT("ASSERT: [%i] %hs:\n'Handle' is not valid, can't dequeue spawn request!"), __LINE__, __FUNCTION__))
	{
		return false;
	}

	const int32 Idx = SpawnQueue.IndexOfByPredicate([&Handle](const FSpawnRequest& Request)
	{
		return Request.Handle == Handle;
	});

	if (!SpawnQueue.IsValidIndex(Idx))
	{
		// 句柄有效但不在队列中，通常是池在清理过程中被清空了
		return false;
	}

	// 复制并移除该请求（不用 Swap 以保持顺序）
	OutRequest = SpawnQueue[Idx];
	SpawnQueue.RemoveAt(Idx);

	return OutRequest.IsValid();
}

// 丢弃队列中所有待处理的生成请求
void UPoolFactory_UObject::ClearSpawnQueue()
{
	SpawnQueue.Empty();
}

// 立即生成请求的对象
UObject* UPoolFactory_UObject::SpawnNow_Implementation(const FSpawnRequest& Request)
{
	return NewObject<UObject>(GetOuter(), Request.GetClassChecked());
}

// 通知所有监听者：对象即将被注册到池中
void UPoolFactory_UObject::OnPreRegistered(const FSpawnRequest& Request, const FPoolObjectData& ObjectData)
{
	if (Request.Callbacks.OnPreRegistered != nullptr)
	{
		Request.Callbacks.OnPreRegistered(ObjectData);
	}
}

// 通知所有监听者：对象已生成
void UPoolFactory_UObject::OnPostSpawned(const FSpawnRequest& Request, const FPoolObjectData& ObjectData)
{
	if (Request.Callbacks.OnPostSpawned != nullptr)
	{
		Request.Callbacks.OnPostSpawned(ObjectData);
	}

	FTakeFromPoolPayload Payload;
	Payload.bIsNewSpawned = true;
	Payload.Transform = Request.Transform;
	OnTakeFromPool(ObjectData.Get(), Payload);

	OnChangedStateInPool(EPoolObjectState::Active, ObjectData.Get());
}

// 于下一帧调用，处理生成队列中的一批请求
void UPoolFactory_UObject::OnNextTickProcessSpawn_Implementation()
{
	int32 ObjectsPerFrame = UPoolManagerSettings::Get().GetSpawnObjectsPerFrame();
	if (!ensureMsgf(ObjectsPerFrame >= 1, TEXT("ASSERT: [%i] %hs:\n'ObjectsPerFrame' is less than 1, set the config!"), __LINE__, __FUNCTION__))
	{
		ObjectsPerFrame = 1;
	}

	const int32 NumToSpawn = FMath::Min(ObjectsPerFrame, SpawnQueue.Num());
	for (int32 Index = 0; Index < NumToSpawn; ++Index)
	{
		FSpawnRequest OutRequest;
		if (DequeueSpawnRequest(OutRequest))
		{
			ProcessRequestNow(OutRequest);
		}
	}

	// 若仍有待生成的 Actor，则安排本函数在下一帧再次调用
	// 之所以延迟到下一帧而不放到其他线程，是因为生成 Actor 并非线程安全操作
	if (!SpawnQueue.IsEmpty())
	{
		const UWorld* World = GetWorld();
		checkf(World, TEXT("ERROR: [%i] %hs:\n'World' is null!"), __LINE__, __FUNCTION__);
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::OnNextTickProcessSpawn);
	}
}

/*********************************************************************************************
 * 销毁
 ********************************************************************************************* */

// 销毁给定对象
void UPoolFactory_UObject::Destroy_Implementation(UObject* Object)
{
	checkf(IsValid(Object), TEXT("ERROR: [%i] %hs:\n'IsValid(Object)' is not valid!"), __LINE__, __FUNCTION__);
	Object->ConditionalBeginDestroy();
}

/*********************************************************************************************
 * 池化
 ********************************************************************************************* */

// 在对象从池中取出之前调用
void UPoolFactory_UObject::OnTakeFromPool_Implementation(UObject* Object, const FTakeFromPoolPayload& Payload)
{
	// 对象实现了该接口时才触发的可选回调
	if (Object && Object->Implements<UPoolObjectCallback>())
	{
		IPoolObjectCallback::Execute_OnTakeFromPool(Object, Payload);
	}
}

// 在对象归还到池之前调用
void UPoolFactory_UObject::OnReturnToPool_Implementation(UObject* Object)
{
	// 对象实现了该接口时才触发的可选回调
	if (Object && Object->Implements<UPoolObjectCallback>())
	{
		IPoolObjectCallback::Execute_OnReturnToPool(Object);
	}
}

// 在对象被取出而激活、或归还而停用时调用
void UPoolFactory_UObject::OnChangedStateInPool_Implementation(EPoolObjectState NewState, UObject* InObject)
{
	// 对象实现了该接口时才触发的可选回调
	if (InObject && InObject->Implements<UPoolObjectCallback>())
	{
		IPoolObjectCallback::Execute_OnChangedStateInPool(InObject, NewState);
	}
}
