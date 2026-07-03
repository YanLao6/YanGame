// 物品定义实现。


#include "Inventory/InventoryItemDefinition.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(InventoryItemDefinition)

UInventoryItemDefinition::UInventoryItemDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

const UInventoryItemFragment* UInventoryItemDefinition::FindFragmentByClass(TSubclassOf<UInventoryItemFragment> FragmentClass) const
{
	if (FragmentClass != nullptr)
	{
		// 逐个扫描定义上的片段，返回第一个类型匹配的片段对象。
		for (UInventoryItemFragment* Fragment : Fragments)
		{
			if (Fragment && Fragment->IsA(FragmentClass))
			{
				return Fragment;
			}
		}
	}

	return nullptr;
}
