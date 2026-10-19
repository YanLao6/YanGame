// Copyright (c) Yevhenii Selivanov

#include "Data/PoolContainer.h"

// Pool Manager
#include "Data/PoolObjectHandle.h"
#include "Factories/PoolFactory_UObject.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PoolContainer)

// 空的池数据容器
const FPoolContainer FPoolContainer::EmptyPool = FPoolContainer();

// 带参构造函数，传入该池的对象类
FPoolContainer::FPoolContainer(const UClass* InClass)
    : ObjectClass(InClass)
{
}

// 按指定对象返回池中对应元素的指针
FPoolObjectData* FPoolContainer::FindInPool(const UObject& Object)
{
	return PoolObjects.FindByKey(&Object);
}

// 按指定句柄返回池中对应元素的指针
FPoolObjectData* FPoolContainer::FindInPool(const FPoolObjectHandle& Handle)
{
	if (!ensureMsgf(Handle.IsValid(), TEXT("ASSERT: [%i] %hs:\n'Handle' is not valid!"), __LINE__, __FUNCTION__))
	{
		return nullptr;
	}

	return PoolObjects.FindByKey(Handle);
}

// 将指定对象从池的注册表中移除
void FPoolContainer::RemoveInPool(const UObject& Object)
{
	if (const FPoolObjectData* ObjectData = FindInPool(Object))
	{
		PoolObjects.RemoveSingleSwap(*ObjectData);
	}
}

// 返回 Factory；未设置时以严重错误崩溃
UPoolFactory_UObject& FPoolContainer::GetFactoryChecked() const
{
	checkf(Factory, TEXT("ERROR: [%i] %hs:\n'Factory' is null!"), __LINE__, __FUNCTION__);
	return *Factory;
}
