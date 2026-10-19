// Copyright (c) Yevhenii Selivanov

#pragma once

#include "UObject/Object.h"

// Pool Manager
#include "Data/PoolObjectData.h"

#include "PoolContainer.generated.h"

/**
 * 按类保存对象，供 Pool Manager 管理。
 */
USTRUCT(BlueprintType)
struct POOLMANAGER_API FPoolContainer
{
	GENERATED_BODY()

	/** 空的池数据容器。 */
	static const FPoolContainer EmptyPool;

	/* 默认构造函数。 */
	FPoolContainer() = default;

	/* 带参构造函数，传入该池的对象类。 */
	explicit FPoolContainer(const UClass* InClass);

	/** 该池中所有对象的类。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient)
	TObjectPtr<const UClass> ObjectClass = nullptr;

	/** 负责管理该池对象的 Factory。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient)
	TObjectPtr<class UPoolFactory_UObject> Factory = nullptr;

	/** 该池中由 Pool Manager 管理的所有对象。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient)
	TArray<FPoolObjectData> PoolObjects;

	/** 按指定对象返回池中对应元素的指针。 */
	FPoolObjectData* FindInPool(const UObject& Object);
	const FORCEINLINE FPoolObjectData* FindInPool(const UObject& Object) const { return const_cast<FPoolContainer*>(this)->FindInPool(Object); }

	/** 按指定句柄返回池中对应元素的指针。 */
	FPoolObjectData* FindInPool(const struct FPoolObjectHandle& Handle);
	const FORCEINLINE FPoolObjectData* FindInPool(const struct FPoolObjectHandle& Handle) const { return const_cast<FPoolContainer*>(this)->FindInPool(Handle); }

	/** 将指定对象从池的注册表中移除。 */
	void RemoveInPool(const UObject& Object);

	/** 返回 Factory；未设置时以严重错误崩溃。 */
	class UPoolFactory_UObject& GetFactoryChecked() const;

	/** 该池已设置对象类时返回 true。 */
	FORCEINLINE bool IsValid() const { return ObjectClass != nullptr; }

	/** 相等运算符，用于查找池 */
	friend POOLMANAGER_API bool operator==(const FPoolContainer& A, const FPoolContainer& B) { return A.ObjectClass == B.ObjectClass; }
	friend POOLMANAGER_API bool operator==(const FPoolContainer& A, const UClass* B) { return A.ObjectClass == B; }
};
