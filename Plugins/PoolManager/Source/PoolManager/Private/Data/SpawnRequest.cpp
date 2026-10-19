// Copyright (c) Yevhenii Selivanov

#include "Data/SpawnRequest.h"

// Pool Manager
#include "Data/PoolObjectData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SpawnRequest)

// 带参构造函数，传入待生成对象的类，自动生成句柄
FSpawnRequest::FSpawnRequest(const UClass* InClass)
    : Handle(InClass)
{
}

// 按指定类和数量生成对应的请求数组
void FSpawnRequest::MakeRequests(TArray<FSpawnRequest>& OutRequests, const UClass* InClass, int32 Amount, ESpawnRequestPriority Priority)
{
	if (!OutRequests.IsEmpty())
	{
		OutRequests.Empty();
	}

	for (int32 Index = 0; Index < Amount; ++Index)
	{
		FSpawnRequest Request(InClass);
		Request.Priority = Priority;
		OutRequests.Emplace(MoveTemp(Request));
	}
}

// 仅保留不在空闲对象列表中的那些请求
void FSpawnRequest::FilterRequests(TArray<FSpawnRequest>& InOutRequests, const TArray<FPoolObjectData>& FreeObjectsData, int32 ExpectedAmount /* = INDEX_NONE*/)
{
	if (!ensureMsgf(!InOutRequests.IsEmpty(), TEXT("ASSERT: [%i] %hs:\n'InOutRequests' is empty!"), __LINE__, __FUNCTION__))
	{
		return;
	}

	InOutRequests.RemoveAll([&FreeObjectsData](const FSpawnRequest& Request)
	{
		return FreeObjectsData.ContainsByPredicate([&Request](const FPoolObjectData& It)
		{
			return It.Handle == Request.Handle;
		});
	});

	ensureMsgf(ExpectedAmount == INDEX_NONE || InOutRequests.Num() == ExpectedAmount,
	    TEXT("ASSERT: [%i] %hs:\nInRequests %i and Expected Amount %i don't equal, something went wrong!"),
	    __LINE__, __FUNCTION__, InOutRequests.Num(), ExpectedAmount);
}
