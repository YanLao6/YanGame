// Copyright (c) Yevhenii Selivanov

#pragma once

// Pool Manager
#include "Factories/PoolFactory_UObject.h"

#include "PoolFactory_UserWidget.generated.h"

/**
 * 负责管理 User Widget，以正确的方式创建和销毁它们。
 */
UCLASS()
class POOLMANAGER_API UPoolFactory_UserWidget : public UPoolFactory_UObject
{
	GENERATED_BODY()

	/*********************************************************************************************
	 * 需要重写的配置项
	 ********************************************************************************************* */
public:
	/** 重写以处理继承自 UserWidget 的类。 */
	virtual const UClass* GetObjectClass_Implementation() const override;

	/*********************************************************************************************
	 * 创建
	 ********************************************************************************************* */
public:
	/** 重写以改用引擎的 'Create Widget' 方式创建 User Widget。 */
	virtual UObject* SpawnNow_Implementation(const FSpawnRequest& Request) override;

	/*********************************************************************************************
	 * 销毁
	 ********************************************************************************************* */
public:
	/** 重写以改用引擎的 RemoveFromParent 方式销毁所管理的 User Widget。 */
	virtual void Destroy_Implementation(UObject* Object) override;

	/*********************************************************************************************
	 * 池化
	 ********************************************************************************************* */
public:
	/** 重写以按新状态改变可见性。 */
	virtual void OnChangedStateInPool_Implementation(EPoolObjectState NewState, UObject* InObject) override;
};
