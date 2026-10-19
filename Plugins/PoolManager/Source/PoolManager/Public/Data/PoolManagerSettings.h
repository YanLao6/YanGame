// Copyright (c) Yevhenii Selivanov

#pragma once

#include "Engine/DeveloperSettings.h"

#include "PoolManagerSettings.generated.h"

class UPoolFactory_UObject;

/**
 * Pool Manager 插件的通用配置数据。
 * 在 'Project Settings' -> "Plugins" -> "Pool Manager" 中设置。
 */
UCLASS(Config = "PoolManager", DefaultConfig, meta = (DisplayName = "Pool Manager"))
class POOLMANAGER_API UPoolManagerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 返回 Pool Manager 插件的 Project Settings 数据。 */
	static const FORCEINLINE UPoolManagerSettings& Get() { return *GetDefault<ThisClass>(); }

	/** 返回 Pool Manager 插件的 Project Settings 数据。 */
	UFUNCTION(BlueprintPure, Category = "[Pool Manager]")
	static const FORCEINLINE UPoolManagerSettings* GetPoolManagerSettings() { return &Get(); }

	//~Begin UDeveloperSettings Interface
	/** 返回配置所属的容器名（Project 或 Editor） */
	virtual FName GetContainerName() const override { return TEXT("Project"); }

	/** 返回配置所属的分类，即 Editor、Engine、Game 等顶层分组 */
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	//~End UDeveloperSettings Interface

	/** 返回每帧最多可生成的 Actor 数量上限。 */
	UFUNCTION(BlueprintPure, Category = "[Pool Manager]")
	int32 GetSpawnObjectsPerFrame() const { return SpawnObjectsPerFrame; }

	/** 返回 Pool Manager 将要使用的全部 Pool Factory。 */
	UFUNCTION(BlueprintPure, Category = "[Pool Manager]")
	void GetPoolFactories(TArray<UClass*>& OutBlueprintPoolFactories) const;

protected:
	/** 设置每帧最多可生成的 Actor 数量上限。 */
	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "[Pool Manager]", meta = (BlueprintProtected = "true"))
	int32 SpawnObjectsPerFrame;

	/** Pool Manager 将要使用的全部 Pool Factory。 */
	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "[Pool Manager]", meta = (BlueprintProtected = "true"))
	TArray<TSoftClassPtr<UPoolFactory_UObject>> PoolFactories;
};
