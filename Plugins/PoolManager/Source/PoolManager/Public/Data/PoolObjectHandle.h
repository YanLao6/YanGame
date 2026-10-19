// Copyright (c) Yevhenii Selivanov

#pragma once

#include "UObject/Object.h"

// UE
#include "Misc/Guid.h"

#include "PoolObjectHandle.generated.h"

/**
 * 用于间接管理池对象的句柄（Handle）。
 * - 提供唯一标识 Hash，与池中的对象一一对应。
 * - 使 Pool Manager 系统内的对象可被追踪和控制。
 * - 适用于向池请求对象后立即拿到句柄的场景：即使对象的生成被延迟到后续帧或其他线程，句柄依然有效。
 */
USTRUCT(BlueprintType)
struct POOLMANAGER_API FPoolObjectHandle
{
	GENERATED_BODY()

	/* 默认构造函数，构造空句柄。 */
	FPoolObjectHandle() = default;

	/** 带参构造函数，传入对象类，自动生成句柄。 */
	explicit FPoolObjectHandle(const UClass* InClass);

	/*********************************************************************************************
	 * 静态辅助函数
	 ********************************************************************************************* */

	/** 空的池对象句柄。 */
	static const FPoolObjectHandle EmptyHandle;

	/** 为指定对象类生成新句柄。 */
	static FPoolObjectHandle NewHandle(const UClass* InObjectClass);

	/** 将生成请求数组转换为句柄数组。 */
	static void Conv_RequestsToHandles(TArray<FPoolObjectHandle>& OutHandles, const TArray<struct FSpawnRequest>& InRequests);

	/** 将池对象数组转换为句柄数组。 */
	static void Conv_ObjectsToHandles(TArray<FPoolObjectHandle>& OutHandles, const TArray<struct FPoolObjectData>& InRequests);

	/*********************************************************************************************
	 * 取值与运算符
	 ********************************************************************************************* */

	/** Hash 已生成时返回 true。 */
	FORCEINLINE bool IsValid() const { return ObjectClass && Hash.IsValid(); }

	/** 清空句柄。 */
	void Invalidate() { *this = EmptyHandle; }

	friend POOLMANAGER_API uint32 GetTypeHash(const FPoolObjectHandle& InHandle) { return GetTypeHash(InHandle.Hash); }
	friend POOLMANAGER_API bool operator==(const FPoolObjectHandle& A, const FPoolObjectHandle& B) { return A.Hash == B.Hash; }

	/*********************************************************************************************
	 * 字段
	 * 字段为私有以防止被直接访问，请改用 NewHandle()。
	 ********************************************************************************************* */
	const UClass* GetObjectClass() const { return ObjectClass; }
	const FGuid& GetHash() const { return Hash; }

private:
	/** 池中对象的类。 */
	UPROPERTY(Transient)
	const UClass* ObjectClass = nullptr;

	/** 为该对象生成的 hash。 */
	FGuid Hash;
};
