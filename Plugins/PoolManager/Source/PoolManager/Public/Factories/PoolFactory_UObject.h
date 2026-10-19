// Copyright (c) Yevhenii Selivanov

#pragma once

#include "UObject/Object.h"

// Pool Manager
#include "Data/SpawnRequest.h"

#include "PoolFactory_UObject.generated.h"

enum class EPoolObjectState : uint8;

/**
 * 每个 Factory 实现自身所负责类（及其子类）对象的创建与管理逻辑。
 * Factory 用于抹平不同类型对象之间的差异：
 * 创建：UObject 用 NewObject；Actor 用 SpawnActor；Component 用 NewObject + RegisterComponent；Widget 用 CreateWidget 等。
 * 销毁：UObject 用 ConditionalBeginDestroy；Actor 用 DestroyActor；Component 用 DestroyComponent；Widget 用 RemoveFromParent 等。
 * 池化：Actor 和 SceneComponent 需要改变可见性、碰撞、Tick 等；UObject 和 Widget 则不需要。
 *
 * 创建新 Factory 的步骤：
 * 1. 继承本类或其子类
 * 2. 在 'Project Settings' -> "Plugins" -> "Pool Manager" -> "Pool Factories" 中添加它
 * 3. 重写 GetObjectClass() 方法。
 */
UCLASS(Blueprintable, BlueprintType)
class POOLMANAGER_API UPoolFactory_UObject : public UObject
{
	GENERATED_BODY()

	/*********************************************************************************************
	 * 需要重写的配置项
	 ********************************************************************************************* */
public:
	/** 子类必须重写，返回该 Factory 负责创建与管理的对象类。
	 * 例如：UPoolFactory_UObject 返回 UObject，UPoolFactory_Actor 返回 AActor，UPoolFactory_Widget 返回 UWidget 等。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "[Pool Manager]")
	const UClass* GetObjectClass() const;
	virtual FORCEINLINE const UClass* GetObjectClass_Implementation() const { return UObject::StaticClass(); }

	/*********************************************************************************************
	 * 创建
	 * 调用顺序：RequestSpawn -> DequeueSpawnRequest -> SpawnNow -> OnPreRegistered -> OnPostSpawned
	 ********************************************************************************************* */
public:
	/** 将对象生成请求加入队列。
	 * 由 UPoolManagerSubsystem::CreateNewObjectInPool 调用。 */
	UFUNCTION(BlueprintNativeEvent, Blueprintable, Category = "[Pool Manager]", meta = (AutoCreateRefTerm = "Request"))
	void RequestSpawn(const FSpawnRequest& Request);
	virtual void RequestSpawn_Implementation(const FSpawnRequest& Request);

	/** 从队列中取出并移除第一个生成请求。
	 * 在 'RequestSpawn' 之后调用。 */
	UFUNCTION(BlueprintCallable, Category = "[Pool Manager]")
	virtual bool DequeueSpawnRequest(FSpawnRequest& OutRequest);

	/** 以给定请求调用 SpawnNow 并处理相应回调。 */
	UFUNCTION(BlueprintCallable, Category = "[Pool Manager]")
	void ProcessRequestNow(const FSpawnRequest& Request);

	/** 立即生成请求的对象。
	 * 在 'DequeueSpawnRequest' 之后调用。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "[Pool Manager]", meta = (AutoCreateRefTerm = "Request"))
	UObject* SpawnNow(const FSpawnRequest& Request);
	virtual UObject* SpawnNow_Implementation(const FSpawnRequest& Request);

	/** 另一种出队方式：按句柄从队列中取出并移除指定的生成请求。 */
	UFUNCTION(BlueprintCallable, Category = "[Pool Manager]")
	virtual bool DequeueSpawnRequestByHandle(const struct FPoolObjectHandle& Handle, FSpawnRequest& OutRequest);

	/** 丢弃队列中所有待处理的生成请求 */
	UFUNCTION(BlueprintCallable, Category = "[Pool Manager]")
	void ClearSpawnQueue();

	/** 生成队列为空（当前没有任何生成请求）时返回 true。 */
	UFUNCTION(BlueprintPure, Category = "[Pool Manager]")
	virtual FORCEINLINE bool IsSpawnQueueEmpty() const { return SpawnQueue.IsEmpty(); }

	/** 在对象生成之后、注册到池之前调用。
	 * 在 'SpawnNow' 之后调用。 */
	UFUNCTION(BlueprintCallable, Category = "[Pool Manager]")
	virtual void OnPreRegistered(const FSpawnRequest& Request, const struct FPoolObjectData& ObjectData);

	/** 在对象生成并注册到池之后调用。
	 * 在 'OnPreRegistered' 之后调用。 */
	UFUNCTION(BlueprintCallable, Category = "[Pool Manager]")
	virtual void OnPostSpawned(const FSpawnRequest& Request, const struct FPoolObjectData& ObjectData);

protected:
	/** 于下一帧调用，处理生成队列中的一批请求。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "[Pool Manager]", meta = (BlueprintProtected))
	void OnNextTickProcessSpawn();
	virtual void OnNextTickProcessSpawn_Implementation();

	/*********************************************************************************************
	 * 销毁
	 ********************************************************************************************* */
public:
	/** 销毁给定对象。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "[Pool Manager]")
	void Destroy(UObject* Object);
	virtual void Destroy_Implementation(UObject* Object);

	/*********************************************************************************************
	 * 池化
	 ********************************************************************************************* */
public:
	/** 在对象从池中取出之前调用。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "[Pool Manager]", meta = (AutoCreateRefTerm = "Payload"))
	void OnTakeFromPool(UObject* Object, const struct FTakeFromPoolPayload& Payload);
	virtual void OnTakeFromPool_Implementation(UObject* Object, const struct FTakeFromPoolPayload& Payload);

	/** 在对象归还到池之前调用。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "[Pool Manager]")
	void OnReturnToPool(UObject* Object);
	virtual void OnReturnToPool_Implementation(UObject* Object);

	/** 在对象被取出而激活、或归还而停用时调用。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "[Pool Manager]")
	void OnChangedStateInPool(EPoolObjectState NewState, UObject* InObject);
	virtual void OnChangedStateInPool_Implementation(EPoolObjectState NewState, UObject* InObject);

	/*********************************************************************************************
	 * 数据
	 ********************************************************************************************* */
protected:
	/** 全部待处理的生成请求。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Transient, AdvancedDisplay, Category = "[Pool Manager]", meta = (BlueprintProtected))
	TArray<FSpawnRequest> SpawnQueue;

	/*********************************************************************************************
	 * 池策略
	 ********************************************************************************************* */
public:
	/** 返回该 Factory 允许缓存的非激活对象数量上限。 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "[Pool Manager]")
	FORCEINLINE int32 GetMaxCachedInactive() const { return MaxCachedInactive; }

protected:
	/**
	 * 该 Factory 允许在池中缓存的非激活对象数量上限。
	 *
	 * 该值仅限制处于 Inactive（缓存）状态的对象数量，
	 * 不限制对象的创建、生成，也不限制激活对象的数量。
	 * 达到上限后，归还的对象将被直接销毁而不再缓存。
	 *
	 * 默认无上限（-1 表示不限制缓存数量）。
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "[Pool Manager]", meta = (BlueprintProtected))
	int32 MaxCachedInactive = INDEX_NONE;
};
