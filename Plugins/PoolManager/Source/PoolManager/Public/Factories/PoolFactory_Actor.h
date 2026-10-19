// Copyright (c) Yevhenii Selivanov

#pragma once

// Pool Manager
#include "PoolFactory_UObject.h"

#include "PoolFactory_Actor.generated.h"

/**
 * 负责管理 Actor，抹平 Actor 相对于普通 UObject 的差异：
 * 创建：调用 SpawnActor。
 * 销毁：调用 DestroyActor。
 * 池化：改变可见性、碰撞、Tick 等。
 */
UCLASS()
class POOLMANAGER_API UPoolFactory_Actor : public UPoolFactory_UObject
{
	GENERATED_BODY()

public:
	/** 停用的 Actor 被移动到的最远位置，与 Iris 复制所依据的 UWorldLocationsConfig::MaxPos 保持一致。 */
	inline static const FVector MaxPos = {+0.5f * 2097152.0f, +0.5f * 2097152.0f, +0.5f * 2097152.0f};

	/*********************************************************************************************
	 * 需要重写的配置项
	 ********************************************************************************************* */
public:
	/** 重写以处理继承自 Actor 的类。 */
	virtual const UClass* GetObjectClass_Implementation() const override;

	/*********************************************************************************************
	 * 创建
	 ********************************************************************************************* */
public:
	/** 重写以改用引擎的 Spawn Actor 方式生成 Actor。 */
	virtual UObject* SpawnNow_Implementation(const FSpawnRequest& Request) override;

	/** 重写以完成 Actor 的延迟生成（spawn 时使用了 deferred construction）。 */
	virtual void OnPreRegistered(const FSpawnRequest& Request, const FPoolObjectData& ObjectData) override;

	/*********************************************************************************************
	 * 销毁
	 ********************************************************************************************* */
public:
	/** 重写以改用引擎的 Destroy Actor 方式销毁给定 Actor。 */
	virtual void Destroy_Implementation(UObject* Object) override;

	/*********************************************************************************************
	 * 池化
	 ********************************************************************************************* */
public:
	/** 重写以在对象从池中取出前设置 Actor 的 Transform。 */
	virtual void OnTakeFromPool_Implementation(UObject* Object, const FTakeFromPoolPayload& Payload) override;

	/** 重写以在对象归还到池前重置 Actor 的 Transform。 */
	virtual void OnReturnToPool_Implementation(UObject* Object) override;

	/** 重写以按新状态改变可见性、碰撞、Tick 等。 */
	virtual void OnChangedStateInPool_Implementation(EPoolObjectState NewState, UObject* InObject) override;
};
