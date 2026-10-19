// 物品初始统计值片段声明。

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Inventory/InventoryItemDefinition.h"
#include "InventoryFragment_SetStats.generated.h"

#define UE_API EQUIPMANAGER_API

class UInventoryItemInstance;

/**
 * 初始统计值片段。
 *
 * 在物品实例创建时把配置好的 Tag 数值写入实例的堆叠计数，
 * 用于弹药上限、备弹量一类随物品定义固定下来的初始数值。
 */
UCLASS(MinimalAPI)
class UInventoryFragment_SetStats : public UInventoryItemFragment
{
	GENERATED_BODY()

protected:
	/** 物品创建时写入实例的初始堆叠数值。 */
	UPROPERTY(EditDefaultsOnly, Category=Equipment)
	TMap<FGameplayTag, int32> InitialItemStats;

public:
	//~Begin UInventoryItemFragment Interface
	UE_API virtual void OnInstanceCreated(UInventoryItemInstance* Instance) const override;
	//~End UInventoryItemFragment Interface

	/** 读取指定 Tag 的初始数值；未配置时返回 0。 */
	UE_API int32 GetItemStatByTag(FGameplayTag Tag) const;
};

#undef UE_API
