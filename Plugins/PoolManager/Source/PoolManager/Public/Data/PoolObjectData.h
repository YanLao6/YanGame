// Copyright (c) Yevhenii Selivanov

#pragma once

#include "UObject/Object.h"

// Pool Manager
#include "Data/PoolObjectHandle.h"

#include "PoolObjectData.generated.h"

enum class EPoolObjectState : uint8;

/**
 * 描述池中某个具体对象的数据。
 */
USTRUCT(BlueprintType)
struct POOLMANAGER_API FPoolObjectData
{
	GENERATED_BODY()

	/* 默认构造函数。 */
	FPoolObjectData() = default;

	/* 带参构造函数，传入要持有的对象。 */
	explicit FPoolObjectData(UObject* InPoolObject);

	/*********************************************************************************************
	 * 静态辅助函数
	 ********************************************************************************************* */

	/** 空的池对象数据。 */
	static const FPoolObjectData EmptyObject;

	/** 转换为对象数组。 */
	static void Conv_PoolDataToObjects(TArray<UObject*>& OutObjects, const TArray<FPoolObjectData>& InPoolData);

	/*********************************************************************************************
	 * 字段
	 ********************************************************************************************* */

	/** 对象已从池中取出时为 true。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient)
	bool bIsActive = false;

	/** 由池管理的对象。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient)
	TObjectPtr<UObject> PoolObject = nullptr;

	/** 与该池对象关联的句柄，用于在 Pool Manager 系统内进行管理。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	FPoolObjectHandle Handle = FPoolObjectHandle::EmptyHandle;

	/*********************************************************************************************
	 * 取值与运算符
	 ********************************************************************************************* */

	/** 对象已从池中取出时返回 true。 */
	FORCEINLINE bool IsActive() const { return bIsActive && IsValid(); }

	/** 返回对象在池中的状态。 */
	EPoolObjectState GetState() const;

	/** 受管对象处于非激活状态、可被取出时返回 true。 */
	FORCEINLINE bool IsFree() const { return !bIsActive && IsValid(); }

	/** 对象已创建时返回 true。 */
	FORCEINLINE bool IsValid() const { return PoolObject && Handle.IsValid(); }

	/** 转换为 "bool"，池对象有效时返回 true。 */
	FORCEINLINE operator bool() const { return IsValid(); }

	/** 元素访问。 */
	template <typename T = UObject>
	FORCEINLINE T* Get() const { return Cast<T>(PoolObject.Get()); }

	/** 元素访问，对象无效时崩溃。 */
	template <typename T = UObject>
	FORCEINLINE T& GetChecked() const { return *CastChecked<T>(PoolObject.Get()); }

	/** 元素访问。 */
	FORCEINLINE UObject* operator->() const { return PoolObject.Get(); }

	/** 相等运算符，用于查找池对象。 */
	friend POOLMANAGER_API bool operator==(const FPoolObjectData& A, const FPoolObjectData& B) { return A.Handle == B.Handle; }
	friend POOLMANAGER_API bool operator==(const FPoolObjectData& A, const FPoolObjectHandle& B) { return A.Handle == B; }
	friend POOLMANAGER_API bool operator==(const FPoolObjectData& A, const UObject* B) { return A.PoolObject == B; }
};
