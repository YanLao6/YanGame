// Copyright (c) Yevhenii Selivanov

#pragma once

#include "UObject/Object.h"

// Pool Manager
#include "Data/PoolObjectHandle.h"
#include "Data/SpawnCallbacks.h"
#include "Data/SpawnRequestPriority.h"

// UE
#include "Templates/NonNullSubclassOf.h"

#include "SpawnRequest.generated.h"

/**
 * 保存生成对象所需信息的结构体。
 */
USTRUCT(BlueprintType, meta = (HasNativeMake = "/Script/PoolManager.PoolManagerUtils.MakeSpawnRequest"))
struct POOLMANAGER_API FSpawnRequest
{
	GENERATED_BODY()

	/** 默认构造函数。 */
	FSpawnRequest() = default;

	/** 带参构造函数，传入对象类，自动生成句柄。 */
	explicit FSpawnRequest(const UClass* InClass);

	/** 按指定类和数量生成对应的请求数组。 */
	static void MakeRequests(TArray<FSpawnRequest>& OutRequests, const UClass* InClass, int32 Amount, ESpawnRequestPriority Priority);

	/** 仅保留不在空闲对象列表中的那些请求。 */
	static void FilterRequests(TArray<FSpawnRequest>& InOutRequests, const TArray<struct FPoolObjectData>& FreeObjectsData, int32 ExpectedAmount = INDEX_NONE);

	/** 待生成对象的 Transform。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient)
	FTransform Transform = FTransform::Identity;

	/** 该生成请求在队列中的优先级，优先级越高越先生成。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient)
	ESpawnRequestPriority Priority = ESpawnRequestPriority::Normal;

	/** 与待生成池对象关联的句柄，用于在 Pool Manager 系统内进行管理。
	 * 未设置时自动生成。 */
	UPROPERTY(BlueprintReadOnly, Transient)
	FPoolObjectHandle Handle = FPoolObjectHandle::EmptyHandle;

	/** 对象生成过程中触发的回调集合。 */
	FSpawnCallbacks Callbacks;

	/** 该生成请求可被处理时返回 true。 */
	FORCEINLINE bool IsValid() const { return Handle.IsValid(); }

	/** 待生成对象的类。 */
	const FORCEINLINE UClass* GetClass() const { return Handle.GetObjectClass(); }

	/** 返回请求的类；未设置或无法转换为指定类型时崩溃。 */
	template <typename T = UObject>
	FORCEINLINE TNonNullSubclassOf<T> GetClassChecked() const { return TNonNullSubclassOf<T>(const_cast<UClass*>(Handle.GetObjectClass())); }
};
