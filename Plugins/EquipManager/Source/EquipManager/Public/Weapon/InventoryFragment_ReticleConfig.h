// 武器准星配置片段声明。

#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryItemDefinition.h"
#include "Templates/SubclassOf.h"
#include "InventoryFragment_ReticleConfig.generated.h"

class UReticleWidgetBase;

/**
 * 准星配置片段。
 *
 * 指定该武器装备时应叠加显示的准星控件，按数组顺序自下而上叠放。
 */
UCLASS(MinimalAPI)
class UInventoryFragment_ReticleConfig : public UInventoryItemFragment
{
	GENERATED_BODY()

public:
	/** 该武器使用的准星控件，按数组顺序叠加显示。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Reticle)
	TArray<TSubclassOf<UReticleWidgetBase>> ReticleWidgets;
};
