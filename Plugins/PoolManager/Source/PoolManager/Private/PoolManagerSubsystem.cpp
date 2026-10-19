// Copyright (c) Yevhenii Selivanov

#include "PoolManagerSubsystem.h"

// Pool Manager
#include "Data/PoolManagerSettings.h"
#include "Data/PoolObjectState.h"
#include "Data/SpawnRequest.h"
#include "Data/TakeFromPoolPayload.h"
#include "Factories/PoolFactory_UObject.h"
#include "PoolManagerUtils.h"

// UE
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFeaturesSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#endif // WITH_EDITOR

#include UE_INLINE_GENERATED_CPP_BY_NAME(PoolManagerSubsystem)

/*********************************************************************************************
 * 静态获取接口
 ********************************************************************************************* */

// 返回 Pool Manager 指针
UPoolManagerSubsystem* UPoolManagerSubsystem::GetPoolManagerByClass(TSubclassOf<UPoolManagerSubsystem> OptionalClass /* = nullptr*/, const UObject* OptionalWorldContext /* = nullptr*/)
{
	if (!OptionalClass)
	{
		OptionalClass = StaticClass();
	}

	const UWorld* World = OptionalWorldContext
	                          ? GEngine->GetWorldFromContextObject(OptionalWorldContext, EGetWorldErrorMode::ReturnNull)
	                          : GEngine->GetCurrentPlayWorld();
#if WITH_EDITOR
	if (!World && GIsEditor && GEditor)
	{
		World = GEditor->IsPlaySessionInProgress()
		            ? (GEditor->GetCurrentPlayWorld() ? GEditor->GetCurrentPlayWorld() : (GEditor->GetPIEWorldContext() ? GEditor->GetPIEWorldContext()->World() : nullptr))
		            : GEditor->GetEditorWorldContext().World();
		World = World ? World : GWorld;
	}
#endif

	UPoolManagerSubsystem* FoundPoolManager = World ? Cast<UPoolManagerSubsystem>(World->GetSubsystemBase(OptionalClass)) : nullptr;
	if (!ensureMsgf(FoundPoolManager, TEXT("%hs: 'Can not find Pool Manager for %s class in %s world"), __FUNCTION__, *OptionalClass->GetName(), *World->GetName()))
	{
		return nullptr;
	}

	return FoundPoolManager;
}

/*********************************************************************************************
 * 从池中取出（单个对象）
 ********************************************************************************************* */

// TakeFromPool() 的异步版本，按指定类返回对象
void UPoolManagerSubsystem::BPTakeFromPool(const UClass* ObjectClass, const FTransform& Transform, const FOnTakenFromPool& Completed, ESpawnRequestPriority Priority)
{
	const FPoolObjectData* ObjectData = TakeFromPoolOrNull(ObjectClass, Transform);
	if (ObjectData)
	{
		// 池中已找到可用对象
		Completed.ExecuteIfBound(ObjectData->PoolObject);
		return;
	}

	FSpawnRequest Request(ObjectClass);
	Request.Transform = Transform;
	Request.Priority = Priority;
	Request.Callbacks.OnPostSpawned = [Completed](const FPoolObjectData& It)
	{
		Completed.ExecuteIfBound(It.PoolObject);
	};
	CreateNewObjectInPool(Request);
}

// TakeFromPool() 的代码侧异步版本，对象就绪时调用回调函数
FPoolObjectHandle UPoolManagerSubsystem::TakeFromPool(const UClass* ObjectClass, const FTransform& Transform /* = FTransform::Identity*/, const FOnSpawnCallback& Completed /* = nullptr*/, ESpawnRequestPriority Priority /* = ESpawnRequestPriority::Normal*/)
{
	const FPoolObjectData* ObjectData = TakeFromPoolOrNull(ObjectClass, Transform);
	if (ObjectData)
	{
		if (Completed != nullptr)
		{
			Completed(*ObjectData);
		}

		return ObjectData->Handle;
	}

	FSpawnRequest Request(ObjectClass);
	Request.Transform = Transform;
	Request.Priority = Priority;
	Request.Callbacks.OnPostSpawned = Completed;
	return CreateNewObjectInPool(Request);
}

// 内部函数：在池中查找对象，找不到则返回 null
const FPoolObjectData* UPoolManagerSubsystem::TakeFromPoolOrNull(const UClass* ObjectClass, const FTransform& Transform /* = FTransform::Identity*/)
{
	if (!ensureMsgf(ObjectClass, TEXT("ASSERT: [%i] %hs:\n'ObjectClass' is not specified!"), __LINE__, __FUNCTION__))
	{
		return nullptr;
	}

	FPoolContainer* Pool = FindPool(ObjectClass);
	if (!Pool)
	{
		// 该池尚未注册，对本函数而言是正常情况，返回 null
		// 由外层负责创建新对象并注册到池中
		return nullptr;
	}

	// 按类在池中查找第一个处于非激活、可被取出的对象
	const FPoolObjectData* FoundData = nullptr;
	for (const FPoolObjectData& DataIt : Pool->PoolObjects)
	{
		if (DataIt.IsFree())
		{
			FoundData = &DataIt;
			break;
		}
	}

	if (!FoundData)
	{
		// 池中没有空闲对象
		return nullptr;
	}

	UObject& InObject = FoundData->GetChecked();

	FTakeFromPoolPayload Payload;
	Payload.bIsNewSpawned = false;
	Payload.Transform = Transform;
	Pool->GetFactoryChecked().OnTakeFromPool(&InObject, Payload);

	SetObjectStateInPool(EPoolObjectState::Active, InObject, *Pool);

	return FoundData;
}

/*********************************************************************************************
 * 从池中取出（多个对象）
 ********************************************************************************************* */

// 与 BPTakeFromPool() 相同，但用于批量获取对象
void UPoolManagerSubsystem::BPTakeFromPoolArray(const UClass* ObjectClass, int32 Amount, const FOnTakenFromPoolArray& Completed, ESpawnRequestPriority Priority)
{
	if (!ensureMsgf(ObjectClass, TEXT("ASSERT: [%i] %hs:\n'ObjectClass' is not specified!"), __LINE__, __FUNCTION__))
	{
		return;
	}

	// --- 优先取用池中已有的空闲对象
	TArray<FSpawnRequest> InRequests;
	FSpawnRequest::MakeRequests(/*out*/ InRequests, ObjectClass, Amount, Priority);
	TArray<FPoolObjectData> FreeObjectsData;
	TakeFromPoolArrayOrNull(/*out*/ FreeObjectsData, InRequests);

	const int32 Difference = InRequests.Num() - FreeObjectsData.Num();
	if (Difference == 0)
	{
		// 所有对象都已从池中取出
		TArray<UObject*> OutObjects;
		FPoolObjectData::Conv_PoolDataToObjects(OutObjects, FreeObjectsData);
		Completed.ExecuteIfBound(OutObjects);
		return;
	}

	// --- 创建剩余不足的对象
	TArray<FPoolObjectHandle> OutHandles;
	FPoolObjectHandle::Conv_ObjectsToHandles(OutHandles, FreeObjectsData);
	FSpawnRequest::FilterRequests(/*out*/ InRequests, FreeObjectsData, Difference);
	CreateNewObjectsArrayInPool(InRequests, OutHandles, [Completed](const TArray<FPoolObjectData>& OutObjects)
	{
		TArray<UObject*> Objects;
		FPoolObjectData::Conv_PoolDataToObjects(Objects, OutObjects);
		Completed.ExecuteIfBound(Objects);
	});
}

// BPTakeFromPoolArray() 的代码版本，可被重写，同类对象全部就绪时调用回调函数
void UPoolManagerSubsystem::TakeFromPoolArray(TArray<FPoolObjectHandle>& OutHandles, const UClass* ObjectClass, int32 Amount, const FOnSpawnAllCallback& Completed, ESpawnRequestPriority Priority /* = ESpawnRequestPriority::Normal*/)
{
	if (!ensureMsgf(ObjectClass, TEXT("ASSERT: [%i] %hs:\n'ObjectClass' is not specified!"), __LINE__, __FUNCTION__)
	    || !ensureMsgf(Amount > 0, TEXT("ASSERT: [%i] %hs:\n'Amount' is less than 1!"), __LINE__, __FUNCTION__))
	{
		return;
	}

	TArray<FSpawnRequest> InRequests;
	FSpawnRequest::MakeRequests(/*out*/ InRequests, ObjectClass, Amount, Priority);
	TArray<FPoolObjectData> FreeObjectsData;
	TakeFromPoolArrayOrNull(/*out*/ FreeObjectsData, InRequests);
	FPoolObjectHandle::Conv_ObjectsToHandles(OutHandles, FreeObjectsData);

	const int32 Difference = InRequests.Num() - FreeObjectsData.Num();
	if (Difference == 0)
	{
		// 所有对象都已从池中取出
		if (Completed)
		{
			Completed(FreeObjectsData);
		}
		return;
	}

	// --- 创建剩余不足的对象
	FSpawnRequest::FilterRequests(/*out*/ InRequests, FreeObjectsData, Difference);
	CreateNewObjectsArrayInPool(InRequests, OutHandles, Completed);
}

// TakeFromPool() 的另一个版本，可一次处理不同类、不同 Transform 的多个请求
void UPoolManagerSubsystem::TakeFromPoolArray(TArray<FPoolObjectHandle>& OutHandles, TArray<FSpawnRequest>& InRequests, const FOnSpawnAllCallback& Completed)
{
	if (!ensureMsgf(!InRequests.IsEmpty(), TEXT("ASSERT: [%i] %hs:\n'InOutRequests' is empty!"), __LINE__, __FUNCTION__))
	{
		return;
	}

	// --- 优先取用池中已有的空闲对象
	TArray<FPoolObjectData> FreeObjectsData;
	TakeFromPoolArrayOrNull(/*out*/ FreeObjectsData, InRequests);
	FPoolObjectHandle::Conv_ObjectsToHandles(OutHandles, FreeObjectsData);

	const int32 Difference = InRequests.Num() - FreeObjectsData.Num();
	if (Difference == 0)
	{
		// 所有对象都已从池中取出
		if (Completed)
		{
			Completed(FreeObjectsData);
		}
		return;
	}

	// --- 创建剩余不足的对象
	FSpawnRequest::FilterRequests(/*out*/ InRequests, FreeObjectsData, Difference);
	CreateNewObjectsArrayInPool(InRequests, OutHandles, Completed);
}

// TakeFromPoolArray() 的同步版本：在池中批量查找可用对象，找不到则不返回
void UPoolManagerSubsystem::TakeFromPoolArrayOrNull(TArray<FPoolObjectData>& OutObjects, TArray<FSpawnRequest>& InRequests)
{
	if (!OutObjects.IsEmpty())
	{
		OutObjects.Empty();
	}

	for (FSpawnRequest& ItRef : InRequests)
	{
		if (const FPoolObjectData* ObjectData = TakeFromPoolOrNull(ItRef.GetClass(), ItRef.Transform))
		{
			ItRef.Handle = ObjectData->Handle;
			OutObjects.Emplace(*ObjectData);
		}
	}
}

/*********************************************************************************************
 * 归还到池（单个对象）
 ********************************************************************************************* */

// 将指定对象归还到池中；若该对象此前是从池中取出的，则同时将其置为非激活
bool UPoolManagerSubsystem::ReturnToPool_Implementation(UObject* Object)
{
	if (!ensureMsgf(Object, TEXT("ASSERT: [%i] %hs:\n'Object' is null!"), __LINE__, __FUNCTION__))
	{
		return false;
	}

	const UClass* ObjectClass = Object->GetClass();
	FPoolContainer* PoolPtr = FindPool(ObjectClass);
	if (!PoolPtr)
	{
		// 从未从池中取出过，或该池已被清空
		return false;
	}

	FPoolContainer& Pool = *PoolPtr;

	/**
	 * 对象已由池管理且处于空闲（非激活）状态。
	 * 重复归还视为空操作。
	 */
	if (const FPoolObjectData* ObjectData = Pool.FindInPool(*Object))
	{
		if (ObjectData->IsFree())
		{
			return true;
		}
	}

	UPoolFactory_UObject& Factory = Pool.GetFactoryChecked();
	const int32 MaxCachedInactive = Factory.GetMaxCachedInactive();
	if (MaxCachedInactive != INDEX_NONE
	    && GetFreeObjectsNum(ObjectClass) >= MaxCachedInactive)
	{
		// Factory 启用了缓存上限且缓存已满，直接丢弃归还的对象
		RemoveObjectInPool(*Object, Pool);
		Factory.Destroy(Object);
		return true;
	}

	Factory.OnReturnToPool(Object);
	SetObjectStateInPool(EPoolObjectState::Inactive, *Object, Pool);

	return true;
}

// ReturnToPool() 的替代版本：按句柄归还对象
bool UPoolManagerSubsystem::ReturnToPool(const FPoolObjectHandle& Handle)
{
	if (!ensureMsgf(Handle.IsValid(), TEXT("ASSERT: [%i] %hs:\n'Handle' is not valid!"), __LINE__, __FUNCTION__))
	{
		return false;
	}

	const FPoolContainer* PoolPtr = FindPool(Handle.GetObjectClass());
	if (!PoolPtr)
	{
		// 从未从池中取出过，或该池已被清空
		return false;
	}

	const FPoolContainer& Pool = *PoolPtr;
	if (const FPoolObjectData* ObjectData = Pool.FindInPool(Handle))
	{
		const bool bSucceed = ReturnToPool(ObjectData->PoolObject);
		return ensureMsgf(bSucceed, TEXT("ASSERT: [%i] %hs:\nFailed to return object to the Pool by given object!"), __LINE__, __FUNCTION__);
	}

	// 这是句柄独有的能力：
	// 若对象归还得比生成还快，则直接取消其生成请求
	FSpawnRequest OutRequest;
	return Pool.GetFactoryChecked().DequeueSpawnRequestByHandle(Handle, OutRequest);
}

/*********************************************************************************************
 * 归还到池（多个对象）
 ********************************************************************************************* */

// 与 ReturnToPool() 相同，但用于批量归还对象
bool UPoolManagerSubsystem::ReturnToPoolArray_Implementation(const TArray<UObject*>& Objects)
{
	bool bSucceed = true;
	for (UObject* ObjectIt : Objects)
	{
		bSucceed &= ReturnToPool(ObjectIt);
	}
	return bSucceed;
}

// 与 ReturnToPool() 相同，但用于批量归还句柄
bool UPoolManagerSubsystem::ReturnToPoolArray(const TArray<FPoolObjectHandle>& Handles)
{
	bool bSucceed = true;
	for (const FPoolObjectHandle& HandleIt : Handles)
	{
		bSucceed &= ReturnToPool(HandleIt);
	}
	return bSucceed;
}

/*********************************************************************************************
 * 进阶接口
 ********************************************************************************************* */

// 按对象所属类，将指定对象原样加入池中，交由 Pool Manager 管理
bool UPoolManagerSubsystem::RegisterObjectInPool_Implementation(const FPoolObjectData& InData, bool bNotify /*= true*/)
{
	if (!ensureMsgf(InData.PoolObject, TEXT("ASSERT: [%i] %hs:\n'PoolObject' is not valid, can't registed it in the Pool!"), __LINE__, __FUNCTION__))
	{
		return false;
	}

	const UClass* ObjectClass = InData.PoolObject.GetClass();
	FPoolContainer& Pool = FindPoolOrAdd(ObjectClass);

	if (Pool.FindInPool(*InData.PoolObject))
	{
		// 池中已包含该对象
		return false;
	}

	FPoolObjectData Data = InData;
	if (!Data.Handle.IsValid())
	{
		// Hash 未设置是正常情况，此处生成一个新的
		Data.Handle = FPoolObjectHandle::NewHandle(ObjectClass);
	}

	Pool.PoolObjects.Emplace(Data);

	SetObjectStateInPool(Data.GetState(), *Data.PoolObject, Pool, bNotify);

	return true;
}

// 始终创建新对象并按其类加入池中
FPoolObjectHandle UPoolManagerSubsystem::CreateNewObjectInPool_Implementation(const FSpawnRequest& InRequest)
{
	if (!ensureMsgf(InRequest.GetClass(), TEXT("ASSERT: [%i] %hs:\n'Class' is not null in the Spawn Request!"), __LINE__, __FUNCTION__))
	{
		return FPoolObjectHandle::EmptyHandle;
	}

	FSpawnRequest Request = InRequest;
	if (!Request.Handle.IsValid())
	{
		// Hash 未设置是正常情况，此处生成一个新的
		Request.Handle = FPoolObjectHandle::NewHandle(Request.GetClass());
	}

	// 新对象一旦生成，总是注册到池中
	const TWeakObjectPtr<ThisClass> WeakThis(this);
	Request.Callbacks.OnPreRegistered = [WeakThis](const FPoolObjectData& ObjectData)
	{
		if (UPoolManagerSubsystem* PoolManager = WeakThis.Get())
		{
			PoolManager->RegisterObjectInPool_Implementation(ObjectData, /*bNotify=*/false);
		}
	};

	const FPoolContainer& Pool = FindPoolOrAdd(Request.GetClass());
	Pool.GetFactoryChecked().RequestSpawn(Request);

	return Request.Handle;
}

// 与 CreateNewObjectInPool() 相同，但用于批量创建对象
void UPoolManagerSubsystem::CreateNewObjectsArrayInPool(TArray<FSpawnRequest>& InRequests, TArray<FPoolObjectHandle>& OutAllHandles, const FOnSpawnAllCallback& Completed /*= nullptr*/)
{
	TArray<FPoolObjectHandle> NewHandles;
	FPoolObjectHandle::Conv_RequestsToHandles(/*out*/ NewHandles, InRequests);
	OutAllHandles.Append(NewHandles);

	// --- 仅在设置了 Completed 时才处理 OnEachSpawned
	FOnSpawnCallback OnEachSpawned = nullptr;
	if (Completed)
	{
		const FPoolObjectHandle& LastHandleRequest = InRequests.Last().Handle;
		const TWeakObjectPtr<const ThisClass> WeakThis(this);
		OnEachSpawned = [WeakThis, OutAllHandles, LastHandleRequest, Completed](const FPoolObjectData& ObjectData)
		{
			const UPoolManagerSubsystem* PoolManager = WeakThis.Get();
			if (!PoolManager
			    || ObjectData.Handle != LastHandleRequest)
			{
				// 尚未生成到最后一个对象
				// 由于队列中请求的顺序被保留，因此可以用 LastHandle 作为判断依据
				return;
			}

			TArray<FPoolObjectData> OutObjects;
			PoolManager->FindPoolObjectsByHandles(OutObjects, OutAllHandles);

			ensureMsgf(OutObjects.Num() == OutAllHandles.Num(), TEXT("ASSERT: [%i] %hs:\n'OutObjects %i != AllHandles %i: It is last Spawn Request is processed, however some of objects failed to spawn or have been destroyed!"), __LINE__, __FUNCTION__, OutObjects.Num(), OutAllHandles.Num());

			Completed(OutObjects);
		};
	}

	for (FSpawnRequest& It : InRequests)
	{
		It.Callbacks.OnPostSpawned = OnEachSpawned;
		CreateNewObjectInPool(It);
	}
}

/*********************************************************************************************
 * 进阶接口 - 工厂
 ********************************************************************************************* */

// 注册新的 Factory，供 Pool Manager 处理指定类及其子类的对象时使用
void UPoolManagerSubsystem::AddFactory(TSubclassOf<UPoolFactory_UObject> FactoryClass)
{
	const UClass* ObjectClass = GetObjectClassByFactory(FactoryClass);
	if (!ensureMsgf(ObjectClass, TEXT("ASSERT: [%i] %hs:\n'ObjectClass' is not set for next factory: %s"), __LINE__, __FUNCTION__, *FactoryClass->GetName())
	    || AllFactories.Contains(ObjectClass))
	{
		return;
	}

	UPoolFactory_UObject* NewFactory = NewObject<UPoolFactory_UObject>(this, FactoryClass);
	AllFactories.Emplace(ObjectClass, NewFactory);
}

// 按类移除 Pool Manager 中的 Factory
void UPoolManagerSubsystem::RemoveFactory(TSubclassOf<UPoolFactory_UObject> FactoryClass)
{
	const UClass* ObjectClass = GetObjectClassByFactory(FactoryClass);
	if (!ensureMsgf(ObjectClass, TEXT("ASSERT: [%i] %hs:\n'ObjectClass' is not set for next factory: %s"), __LINE__, __FUNCTION__, *FactoryClass->GetName()))
	{
		return;
	}

	const TObjectPtr<UPoolFactory_UObject>* FactoryPtr = AllFactories.Find(ObjectClass);
	if (!ensureMsgf(FactoryPtr, TEXT("ASSERT: [%i] %hs:\nFactory is not found for next class: %s"), __LINE__, __FUNCTION__, *ObjectClass->GetName()))
	{
		return;
	}

	UPoolFactory_UObject* Factory = *FactoryPtr;
	if (IsValid(Factory))
	{
		Factory->ConditionalBeginDestroy();
	}

	AllFactories.Remove(ObjectClass);
}

// 沿类继承链向上查找，返回处理该对象类型（或其祖先类）的最近一个已注册 Factory
UPoolFactory_UObject* UPoolManagerSubsystem::FindPoolFactoryChecked(const UClass* ObjectClass) const
{
	checkf(ObjectClass, TEXT("ERROR: [%i] %hs:\n'ObjectClass' is null!"), __LINE__, __FUNCTION__);

	const TObjectPtr<UPoolFactory_UObject>* FoundFactory = nullptr;
	const UClass* CurrentClass = ObjectClass;

	// 持续沿继承链向上查找，直到找到已注册的 Factory 或到达根类
	while (CurrentClass != nullptr)
	{
		FoundFactory = AllFactories.Find(CurrentClass);
		if (FoundFactory)
		{
			break; // 找到 Factory 即退出循环
		}
		CurrentClass = CurrentClass->GetSuperClass(); // 否则继续向上查找父类
	}

	checkf(FoundFactory, TEXT("ERROR: [%i] %hs:\n'FoundFactory' is null for next object class: %s"), __LINE__, __FUNCTION__, *GetNameSafe(ObjectClass));
	return *FoundFactory;
}

// 返回指定 Factory 所负责处理的默认对象类
const UClass* UPoolManagerSubsystem::GetObjectClassByFactory(const TSubclassOf<UPoolFactory_UObject>& FactoryClass)
{
	if (!ensureMsgf(FactoryClass, TEXT("ASSERT: [%i] %hs:\n'FactoryClass' is null!"), __LINE__, __FUNCTION__))
	{
		return nullptr;
	}

	const UPoolFactory_UObject* FactoryCDO = CastChecked<UPoolFactory_UObject>(FactoryClass->GetDefaultObject());
	return FactoryCDO->GetObjectClass();
}

// 创建 Pool Manager 处理对象时可能用到的全部 Pool Factory
void UPoolManagerSubsystem::InitializeAllFactories()
{
	TArray<UClass*> AllPoolFactories;
	UPoolManagerSettings::Get().GetPoolFactories(/*out*/ AllPoolFactories);
	for (UClass* FactoryClass : AllPoolFactories)
	{
		AddFactory(FactoryClass);
	}
}

// 销毁 Pool Manager 处理对象时使用的全部 Pool Factory
void UPoolManagerSubsystem::ClearAllFactories()
{
	for (const TTuple<TObjectPtr<const UClass>, TObjectPtr<UPoolFactory_UObject>>& FactoryIt : AllFactories)
	{
		if (IsValid(FactoryIt.Value))
		{
			FactoryIt.Value->ConditionalBeginDestroy();
		}
	}

	AllFactories.Empty();
}

/*********************************************************************************************
 * 清空池
 ********************************************************************************************* */

// 销毁指定类对应池中的所有对象
void UPoolManagerSubsystem::EmptyPool_Implementation(const UClass* ObjectClass)
{
	const int32 PoolIdx = ObjectClass ? Pools.IndexOfByKey(ObjectClass) : INDEX_NONE;
	if (!ensureMsgf(PoolIdx != INDEX_NONE, TEXT("ASSERT: [%i] %hs:\n'ObjectClass' %s is not contained in the pool!"), __LINE__, __FUNCTION__, *GetNameSafe(ObjectClass)))
	{
		return;
	}

	FPoolContainer& Pool = Pools[PoolIdx];
	UPoolFactory_UObject& Factory = Pool.GetFactoryChecked();

	// 丢弃延迟生成的请求，避免池被移除后又被重新创建
	Factory.ClearSpawnQueue();

	TArray<FPoolObjectData>& PoolObjects = Pool.PoolObjects;
	for (int32 Index = PoolObjects.Num() - 1; Index >= 0; --Index)
	{
		if (!PoolObjects.IsValidIndex(Index))
		{
			continue;
		}

		UObject* ObjectIt = PoolObjects[Index].Get();
		if (!IsValid(ObjectIt))
		{
			continue;
		}

		if (PoolObjects[Index].bIsActive)
		{
			// 先把激活对象归还到池，确保销毁前能正常触发停用回调
			Factory.OnReturnToPool(ObjectIt);
		}

		Factory.Destroy(ObjectIt);
	}

	PoolObjects.Empty();

	Pools.RemoveAtSwap(PoolIdx);
}

// 销毁 Pool Manager 管理的所有池中的所有对象
void UPoolManagerSubsystem::EmptyAllPools_Implementation()
{
	const int32 PoolsNum = Pools.Num();
	for (int32 Index = PoolsNum - 1; Index >= 0; --Index)
	{
		const UClass* ObjectClass = Pools.IsValidIndex(Index) ? Pools[Index].ObjectClass : nullptr;
		EmptyPool(ObjectClass);
	}

	Pools.Empty();
}

// 按谓词（Predicate）销毁 Pool Manager 中符合条件的所有对象
void UPoolManagerSubsystem::EmptyAllByPredicate(const TFunctionRef<bool(const UObject* Object)> Predicate)
{
	const int32 PoolsNum = Pools.Num();
	for (int32 PoolIndex = PoolsNum - 1; PoolIndex >= 0; --PoolIndex)
	{
		if (!Pools.IsValidIndex(PoolIndex))
		{
			continue;
		}

		FPoolContainer& PoolIt = Pools[PoolIndex];
		UPoolFactory_UObject& Factory = PoolIt.GetFactoryChecked();
		TArray<FPoolObjectData>& PoolObjectsRef = PoolIt.PoolObjects;

		const int32 ObjectsNum = PoolObjectsRef.Num();
		for (int32 ObjectIndex = ObjectsNum - 1; ObjectIndex >= 0; --ObjectIndex)
		{
			UObject* ObjectIt = PoolObjectsRef.IsValidIndex(ObjectIndex) ? PoolObjectsRef[ObjectIndex].Get() : nullptr;
			if (!IsValid(ObjectIt)
			    || !Predicate(ObjectIt))
			{
				continue;
			}

			Factory.Destroy(ObjectIt);

			PoolObjectsRef.RemoveAt(ObjectIndex);
		}
	}
}

/*********************************************************************************************
 * 查询接口
 ********************************************************************************************* */

// 返回指定对象当前的状态
EPoolObjectState UPoolManagerSubsystem::GetPoolObjectState_Implementation(const UObject* Object) const
{
	const UClass* ObjectClass = Object ? Object->GetClass() : nullptr;
	const FPoolContainer* Pool = FindPool(ObjectClass);
	const FPoolObjectData* PoolObject = Pool ? Pool->FindInPool(*Object) : nullptr;

	if (!PoolObject
	    || !PoolObject->IsValid())
	{
		// 不在任何池中
		return EPoolObjectState::None;
	}

	return PoolObject->GetState();
}

// 指定对象是否由 Pool Manager 管理
bool UPoolManagerSubsystem::ContainsObjectInPool_Implementation(const UObject* Object) const
{
	return GetPoolObjectState(Object) != EPoolObjectState::None;
}

bool UPoolManagerSubsystem::ContainsClassInPool_Implementation(const UClass* ObjectClass) const
{
	return FindPool(ObjectClass) != nullptr;
}

// 指定对象是否由 Pool Manager 管理且已从池中取出
bool UPoolManagerSubsystem::IsActive_Implementation(const UObject* Object) const
{
	return GetPoolObjectState(Object) == EPoolObjectState::Active;
}

// 受管对象是否处于非激活状态、可被取出
bool UPoolManagerSubsystem::IsFreeObjectInPool_Implementation(const UObject* Object) const
{
	return GetPoolObjectState(Object) == EPoolObjectState::Inactive;
}

// 返回指定类的池中空闲对象数量
int32 UPoolManagerSubsystem::GetFreeObjectsNum_Implementation(const UClass* ObjectClass) const
{
	const FPoolContainer* Pool = FindPool(ObjectClass);
	if (!Pool)
	{
		return 0;
	}

	int32 FreeObjectsNum = 0;
	for (const FPoolObjectData& PoolObjectIt : Pool->PoolObjects)
	{
		if (PoolObjectIt.IsFree())
		{
			++FreeObjectsNum;
		}
	}
	return FreeObjectsNum;
}

// 指定对象是否已被 Pool Manager 记录
bool UPoolManagerSubsystem::IsRegistered_Implementation(const UObject* Object) const
{
	return GetPoolObjectState(Object) != EPoolObjectState::None;
}

// 返回指定类的池中已注册对象数量
int32 UPoolManagerSubsystem::GetRegisteredObjectsNum_Implementation(const UClass* ObjectClass) const
{
	const FPoolContainer* Pool = FindPool(ObjectClass);
	if (!Pool)
	{
		return 0;
	}

	int32 RegisteredObjectsNum = 0;
	for (const FPoolObjectData& PoolObjectIt : Pool->PoolObjects)
	{
		if (PoolObjectIt.IsValid())
		{
			++RegisteredObjectsNum;
		}
	}

	return RegisteredObjectsNum;
}

// 返回与给定句柄关联的对象
const FPoolObjectData& UPoolManagerSubsystem::FindPoolObjectByHandle(const FPoolObjectHandle& Handle) const
{
	const FPoolContainer* Pool = FindPool(Handle.GetObjectClass());
	const FPoolObjectData* ObjectData = Pool ? Pool->FindInPool(Handle) : nullptr;
	return ObjectData ? *ObjectData : FPoolObjectData::EmptyObject;
}

// 返回与给定对象关联的句柄
const FPoolObjectHandle& UPoolManagerSubsystem::FindPoolHandleByObject(const UObject* Object) const
{
	const FPoolContainer* Pool = Object ? FindPool(Object->GetClass()) : nullptr;
	const FPoolObjectData* ObjectData = Pool ? Pool->FindInPool(*Object) : nullptr;
	return ObjectData ? ObjectData->Handle : FPoolObjectHandle::EmptyHandle;
}

// 从给定的所有句柄中筛选出有效的对象
void UPoolManagerSubsystem::FindPoolObjectsByHandles(TArray<FPoolObjectData>& OutObjects, const TArray<FPoolObjectHandle>& InHandles) const
{
	for (const FPoolObjectHandle& HandleIt : InHandles)
	{
		FPoolObjectData PoolObject = FindPoolObjectByHandle(HandleIt);
		if (PoolObject.IsValid())
		{
			OutObjects.Emplace(MoveTemp(PoolObject));
		}
	}
}

/*********************************************************************************************
 * 受保护方法
 ********************************************************************************************* */

// 在 Pool Manager 实例初始化时调用
void UPoolManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	InitializeAllFactories();

	UGameFeaturesSubsystem::Get().AddObserver(this, UGameFeaturesSubsystem::EObserverPluginStateUpdateMode::FutureOnly);

#if WITH_EDITOR
	if (GEditor
	    && !GEditor->IsPlaySessionInProgress() // 处于编辑器且不在 PIE 中
	    && !GEditor->OnWorldDestroyed().IsBoundToObject(this))
	{
		// 编辑器下的 Pool Manager 实例与 PIE 下的实例生命周期不同，
		// 因此在编辑器切换关卡时清空所有池，避免内存泄漏
		const TWeakObjectPtr<ThisClass> WeakThis(this);
		auto OnWorldDestroyed = [WeakThis](const UWorld* World)
		{
			if (!World || !World->IsEditorWorld()
			    || !GEditor || GEditor->IsPlaySessionInProgress())
			{
				// 不是编辑器 World，或正处于 PIE 中
				return;
			}

			if (UPoolManagerSubsystem* PoolManager = WeakThis.Get())
			{
				PoolManager->EmptyAllPools();
			}
		};

		GEditor->OnWorldDestroyed().AddWeakLambda(this, OnWorldDestroyed);
	}
#endif // WITH_EDITOR
}

// 在 Pool Manager 实例反初始化时调用
void UPoolManagerSubsystem::Deinitialize()
{
	Super::Deinitialize();

	UGameFeaturesSubsystem::Get().RemoveObserver(this);

	ClearAllFactories();
}

// 自动清空归属于正在停用的 game feature 插件的那些类所对应的池
void UPoolManagerSubsystem::OnGameFeatureDeactivating(const UGameFeatureData* GameFeatureData, FGameFeatureDeactivatingContext& Context, const FString& PluginURL)
{
	for (int32 Index = Pools.Num() - 1; Index >= 0; --Index)
	{
		if (Pools.IsValidIndex(Index)
		    && UPoolManagerUtils::IsPoolInGameFeaturePlugin(Pools[Index].ObjectClass, GameFeatureData))
		{
			EmptyPool(Pools[Index].ObjectClass);
		}
	}
}

// 按指定类返回找到的池指针，不存在则新建
FPoolContainer& UPoolManagerSubsystem::FindPoolOrAdd(const UClass* ObjectClass)
{
	checkf(ObjectClass, TEXT("ERROR: [%i] %hs:\n'ObjectClass' is null!"), __LINE__, __FUNCTION__);

	if (FPoolContainer* Pool = FindPool(ObjectClass))
	{
		return *Pool;
	}

	FPoolContainer& Pool = Pools.AddDefaulted_GetRef();
	Pool.ObjectClass = ObjectClass;
	Pool.Factory = FindPoolFactoryChecked(ObjectClass);
	return Pool;
}

// 按指定类返回找到的池指针
FPoolContainer* UPoolManagerSubsystem::FindPool(const UClass* ObjectClass)
{
	if (!ensureMsgf(ObjectClass, TEXT("ASSERT: [%i] %hs:\n'ObjectClass' is null!"), __LINE__, __FUNCTION__))
	{
		return nullptr;
	}

	return Pools.FindByKey(ObjectClass);
}

// 若对象由 Pool Manager 管理，则激活或停用该对象
void UPoolManagerSubsystem::SetObjectStateInPool(EPoolObjectState NewState, UObject& InObject, FPoolContainer& InPool, bool bNotify /*= true*/)
{
	FPoolObjectData* PoolObject = InPool.FindInPool(InObject);
	if (!ensureMsgf(PoolObject && PoolObject->IsValid(), TEXT("ASSERT: [%i] %hs:\n'PoolObject' is not registered in given pool for class: %s"), __LINE__, __FUNCTION__, *GetNameSafe(InPool.ObjectClass)))
	{
		return;
	}

	PoolObject->bIsActive = NewState == EPoolObjectState::Active;

	if (bNotify)
	{
		InPool.GetFactoryChecked().OnChangedStateInPool(NewState, &InObject);
	}
}

// 将对象从池的注册表中注销，并从内部池数据中移除
void UPoolManagerSubsystem::RemoveObjectInPool(UObject& InObject, FPoolContainer& InPool)
{
	const FPoolObjectData* PoolObject = InPool.FindInPool(InObject);
	if (!ensureMsgf(PoolObject && PoolObject->IsValid(), TEXT("ASSERT: [%i] %hs:\nObject is not registered or has invalid pool data for class: %s"), __LINE__, __FUNCTION__, *GetNameSafe(InPool.ObjectClass)))
	{
		return;
	}

	InPool.RemoveInPool(InObject);
}
