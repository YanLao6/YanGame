// 局内开局配装定义实现。

#include "Loadout/InGameLoadoutDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InGameLoadoutDefinition)

bool UInGameLoadoutDefinition::IsSlotOverridable(int32 SlotIndex) const
{
	if (SlotIndex < 0)
	{
		return false;
	}

	switch (OverridePolicy)
	{
		case ELoadoutOverridePolicy::AllSlots:
		{
			return true;
		}
		case ELoadoutOverridePolicy::ListedSlots:
		{
			return OverridableSlots.Contains(SlotIndex);
		}
		default:
		{
			return false;
		}
	}
}

bool UInGameLoadoutDefinition::IsItemAllowed(FName ItemId) const
{
	if (ItemId.IsNone())
	{
		return false;
	}

	// 名单为空即不限制，这样只想开放槽位而不挑物品的模式无须逐个登记。
	if (AllowedItemIds.IsEmpty())
	{
		return true;
	}

	return AllowedItemIds.Contains(ItemId);
}
