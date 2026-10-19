// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Inventory/InventoryItemDefinition.h"
#include "Styling/SlateBrush.h"

#include "InventoryFragment_QuickBarIcon.generated.h"

class UObject;

/** 快捷栏图标片段：描述物品在快捷栏槽位上的展示外观（物品图标与弹药图标）。 */
UCLASS(MinimalAPI)
class UInventoryFragment_QuickBarIcon : public UInventoryItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateBrush Brush;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateBrush AmmoBrush;
};
