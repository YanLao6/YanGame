// 物品初始统计值片段实现。

#include "Inventory/InventoryFragment_SetStats.h"

#include "Inventory/InventoryItemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InventoryFragment_SetStats)

void UInventoryFragment_SetStats::OnInstanceCreated(UInventoryItemInstance* Instance) const
{
	for (const TPair<FGameplayTag, int32>& KVP : InitialItemStats)
	{
		Instance->AddStatTagStack(KVP.Key, KVP.Value);
	}
}

int32 UInventoryFragment_SetStats::GetItemStatByTag(FGameplayTag Tag) const
{
	if (const int32* StatPtr = InitialItemStats.Find(Tag))
	{
		return *StatPtr;
	}

	return 0;
}
