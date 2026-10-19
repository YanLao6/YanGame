// Copyright (c) Yevhenii Selivanov

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

// Pool Manager
#include "Data/SpawnRequestPriority.h"

#include "PoolManagerUtils.generated.h"

/**
 * Pool Manager 的蓝图工具类。
 * 为蓝图提供辅助函数。
 */
UCLASS()
class POOLMANAGER_API UPoolManagerUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 创建池对象的生成请求。
	 * 该函数会以有效的句柄正确初始化 FSpawnRequest。
	 * @param ObjectClass 要从池中生成的对象类。
	 * @param Transform 生成对象所用的 Transform。
	 * @param Priority 该生成请求在队列中的优先级。
	 * @return 已正确初始化、可直接使用的生成请求。 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "[Pool Manager]", meta = (NativeMakeFunc, AutoCreateRefTerm = "Transform"))
	static struct FSpawnRequest MakeSpawnRequest(TSubclassOf<UObject> ObjectClass, const FTransform& Transform, ESpawnRequestPriority Priority = ESpawnRequestPriority::Normal);

	/** 给定类是否属于 GameFeatureData 所标识的 game feature 插件。
	 * 通过比较类所属模块名与插件内容根目录来判断 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "[Pool Manager]")
	static bool IsPoolInGameFeaturePlugin(const UClass* ObjectClass, const class UGameFeatureData* GameFeatureData);
};
