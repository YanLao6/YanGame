// Copyright (c) Yevhenii Selivanov

#include "Data/PoolObjectHandle.h"

// Pool Manager
#include "Data/PoolObjectData.h"
#include "Data/SpawnRequest.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PoolObjectHandle)

// 空的池对象句柄
const FPoolObjectHandle FPoolObjectHandle::EmptyHandle = FPoolObjectHandle();

// 带参构造函数，传入对象类，自动生成句柄
FPoolObjectHandle::FPoolObjectHandle(const UClass* InClass)
{
	*this = NewHandle(InClass);
}

// 为指定对象类生成新句柄
FPoolObjectHandle FPoolObjectHandle::NewHandle(const UClass* InObjectClass)
{
	if (!ensureMsgf(InObjectClass, TEXT("ASSERT: [%i] %hs:\n'InObjectClass' is null, can't generate new handle!"), __LINE__, __FUNCTION__))
	{
		return EmptyHandle;
	}

	FPoolObjectHandle Handle;
	Handle.ObjectClass = InObjectClass;
	Handle.Hash = FGuid::NewGuid();
	return Handle;
}

// 将生成请求数组转换为句柄数组
void FPoolObjectHandle::Conv_RequestsToHandles(TArray<FPoolObjectHandle>& OutHandles, const TArray<FSpawnRequest>& InRequests)
{
	if (!OutHandles.IsEmpty())
	{
		OutHandles.Empty();
	}

	OutHandles.Reserve(InRequests.Num());
	for (const FSpawnRequest& Request : InRequests)
	{
		OutHandles.Emplace(Request.Handle);
	}
}

// 将池对象数组转换为句柄数组
void FPoolObjectHandle::Conv_ObjectsToHandles(TArray<FPoolObjectHandle>& OutHandles, const TArray<FPoolObjectData>& InRequests)
{
	if (!OutHandles.IsEmpty())
	{
		OutHandles.Empty();
	}

	OutHandles.Reserve(InRequests.Num());
	for (const FPoolObjectData& It : InRequests)
	{
		OutHandles.Emplace(It.Handle);
	}
}
