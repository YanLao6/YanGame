// Copyright (c) Yevhenii Selivanov

#include "Data/PoolObjectData.h"

// Pool Manager
#include "Data/PoolObjectState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PoolObjectData)

// 空的池对象数据
const FPoolObjectData FPoolObjectData::EmptyObject = FPoolObjectData();

// 转换为对象数组
void FPoolObjectData::Conv_PoolDataToObjects(TArray<UObject*>& OutObjects, const TArray<FPoolObjectData>& InPoolData)
{
	if (!OutObjects.IsEmpty())
	{
		OutObjects.Empty();
	}

	OutObjects.Reserve(InPoolData.Num());
	for (const FPoolObjectData& It : InPoolData)
	{
		OutObjects.Add(It.PoolObject);
	}
}

// 带参构造函数，传入要持有的对象
FPoolObjectData::FPoolObjectData(UObject* InPoolObject)
    : PoolObject(InPoolObject)
{
}

// 返回对象在池中的状态
EPoolObjectState FPoolObjectData::GetState() const
{
	return bIsActive ? EPoolObjectState::Active : EPoolObjectState::Inactive;
}
