// Copyright (c) Lim Young & Yevhenii Selivanov

#pragma once

#include "UObject/Interface.h"
#include "PoolObjectCallback.generated.h"

enum class EPoolObjectState : uint8;

UINTERFACE()
class UPoolObjectCallback : public UInterface
{
	GENERATED_BODY()
};

/**
 * 让池对象自身能够接收 Pool Manager 的事件。
 * 在池对象类中实现该接口即可收到 Pool Manager 的事件回调。
 */
class POOLMANAGER_API IPoolObjectCallback
{
	GENERATED_BODY()

public:
	/**
	 * 对象从池中取出时调用。若 Payload 的 bIsNewSpawned 为 true，说明该对象是新生成的。
	 * @param Payload 取出对象时携带的负载数据。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "[Pool Manager]", meta = (AutoCreateRefTerm = "Payload"))
	void OnTakeFromPool(const struct FTakeFromPoolPayload& Payload);
	virtual void OnTakeFromPool_Implementation(const FTakeFromPoolPayload& Payload) {}

	/**
	 * 对象归还到池时调用。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "[Pool Manager]")
	void OnReturnToPool();
	virtual void OnReturnToPool_Implementation() {}

	/**
	 * 对象被激活或停用时调用。
	 * @param NewState 对象的新状态。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "[Pool Manager]")
	void OnChangedStateInPool(EPoolObjectState NewState);
	virtual void OnChangedStateInPool_Implementation(EPoolObjectState NewState) {}
};
