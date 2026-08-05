// 可装备物品片段声明。

#pragma once

#include "CoreMinimal.h"
#include "InventoryItemDefinition.h"
#include "Templates/SubclassOf.h"
#include "InventoryFragment_EquippableItem.generated.h"


class UEquipmentDefinition;

/**
 * 可装备物品片段。
 *
 * 用于把物品定义映射到具体的装备定义，使背包物品能够被快捷栏和装备系统识别并生成对应装备实例。
 */
UCLASS(MinimalAPI)
class UInventoryFragment_EquippableItem : public UInventoryItemFragment
{
	GENERATED_BODY()

public:
	/** 该物品被装备时应使用的装备定义类型。 */
	UPROPERTY(EditAnywhere, Category=EM)
	TSubclassOf<UEquipmentDefinition> EquipmentDefinition;
};
