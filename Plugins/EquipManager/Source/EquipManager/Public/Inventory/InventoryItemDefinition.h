// 物品定义声明。

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "InventoryItemDefinition.generated.h"

class UInventoryItemInstance;
struct FFrame;

/**
 * 物品片段基类。
 *
 * 用于以组合方式为物品定义附加额外功能和静态配置。
 */
UCLASS(MinimalAPI, DefaultToInstanced, EditInlineNew, Abstract)
class UInventoryItemFragment : public UObject
{
	GENERATED_BODY()

public:
	/** 当物品实例基于本片段创建完成后触发，可用于写入初始状态。 */
	virtual void OnInstanceCreated(UInventoryItemInstance* Instance) const {}
};

/**
 * 物品定义。
 *
 * 只保存静态配置数据，运行时会根据它创建对应的物品实例。
 */
UCLASS(MinimalAPI, Blueprintable, Const, Abstract)
class UInventoryItemDefinition : public UObject
{
	GENERATED_BODY()

public:
	/** 构造物品定义对象。 */
	UInventoryItemDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 物品在界面中显示的名称。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Display)
	FText DisplayName;

	/** 通过片段组合出的其他静态配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Display, Instanced)
	TArray<TObjectPtr<UInventoryItemFragment>> Fragments;

public:
	/** 在当前物品定义中查找第一个匹配类型的片段。 */
	const UInventoryItemFragment* FindFragmentByClass(TSubclassOf<UInventoryItemFragment> FragmentClass) const;
};
