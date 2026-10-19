// 配装决议实现。

#include "Loadout/LoadoutResolver.h"

#include "Loadout/InGameLoadoutDefinition.h"
#include "ModularGameplayAbilitiesLogChannels.h"
#include "Stash/InventoryItemCatalog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LoadoutResolver)

namespace LoadoutResolver
{
	namespace
	{
		// 把一条发放并入结果：占槽的条目按槽位顶替，只进背包的条目直接追加。
		void MergeEntry(FResolvedLoadout& Result, const FResolvedLoadoutEntry& Entry)
		{
			if (Entry.QuickBarSlot < 0)
			{
				Result.Entries.Add(Entry);
				return;
			}

			FResolvedLoadoutEntry* ExistingEntry = Result.Entries.FindByPredicate(
				[&Entry](const FResolvedLoadoutEntry& Candidate) { return Candidate.QuickBarSlot == Entry.QuickBarSlot; });

			if (ExistingEntry != nullptr)
			{
				*ExistingEntry = Entry;
			}
			else
			{
				Result.Entries.Add(Entry);
			}
		}

		// 任一份配装允许即允许：各层只声明自己开放什么，不必顾及别层的收紧。
		bool IsSlotOverridable(const TArray<const UInGameLoadoutDefinition*>& Definitions, int32 SlotIndex)
		{
			return Definitions.ContainsByPredicate(
				[SlotIndex](const UInGameLoadoutDefinition* Definition)
				{
					return (Definition != nullptr) && Definition->IsSlotOverridable(SlotIndex);
				});
		}

		bool IsItemAllowed(const TArray<const UInGameLoadoutDefinition*>& Definitions, FName ItemId)
		{
			return Definitions.ContainsByPredicate(
				[ItemId](const UInGameLoadoutDefinition* Definition)
				{
					return (Definition != nullptr) && Definition->IsItemAllowed(ItemId);
				});
		}
	}

	FResolvedLoadout Resolve(const TArray<const UInGameLoadoutDefinition*>& Definitions, const FLoadoutRequest& Request, const UInventoryItemCatalog* Catalog)
	{
		FResolvedLoadout Result;

		if (Definitions.IsEmpty())
		{
			UE_LOG(LogModularGameplayAbilities, Warning, TEXT("配装决议未拿到任何配装资产，本局不发放任何物品。"));
			return Result;
		}

		// 先铺底：各份配装按顺序叠加，决定这一局至少有什么。
		for (const UInGameLoadoutDefinition* Definition : Definitions)
		{
			if (Definition == nullptr)
			{
				continue;
			}

			int32 LayerFirstSlot = INDEX_NONE;

			for (const FInGameLoadoutGrant& Grant : Definition->BaseGrants)
			{
				if (Grant.ItemDefinition.IsNull())
				{
					continue;
				}

				FResolvedLoadoutEntry Entry;
				Entry.ItemDefinition = Grant.ItemDefinition;
				Entry.StackCount     = FMath::Max(1, Grant.StackCount);
				Entry.QuickBarSlot   = Grant.QuickBarSlot;

				MergeEntry(Result, Entry);

				if (Grant.QuickBarSlot >= 0)
				{
					LayerFirstSlot = (LayerFirstSlot == INDEX_NONE) ? Grant.QuickBarSlot : FMath::Min(LayerFirstSlot, Grant.QuickBarSlot);
				}
			}

			// 激活权归声明要激活的那一层，激活的是它自己占用的首个槽位；
			// 多层都声明时以后加入者为准，英雄层因此能覆盖模式层的选择。
			if (Definition->bActivateFirstOccupiedSlot && (LayerFirstSlot != INDEX_NONE))
			{
				Result.SlotToActivate = LayerFirstSlot;
			}
		}

		TSet<int32> OverriddenSlots;

		for (const FLoadoutRequestEntry& RequestEntry : Request.Entries)
		{
			if (!IsSlotOverridable(Definitions, RequestEntry.SlotIndex))
			{
				UE_LOG(LogModularGameplayAbilities, Warning, TEXT("配装决议：槽位 %d 不允许覆盖，该条请求被丢弃。"), RequestEntry.SlotIndex);
				continue;
			}

			if (!IsItemAllowed(Definitions, RequestEntry.ItemId))
			{
				UE_LOG(LogModularGameplayAbilities, Warning, TEXT("配装决议：物品 [%s] 不在本模式的准入名单内，该条请求被丢弃。"), *RequestEntry.ItemId.ToString());
				continue;
			}

			if (Catalog == nullptr)
			{
				UE_LOG(LogModularGameplayAbilities, Warning, TEXT("配装决议：没有物品目录，无法解析标识 [%s]。"), *RequestEntry.ItemId.ToString());
				continue;
			}

			const TSoftClassPtr<UInventoryItemDefinition> ItemDefinition = Catalog->FindItemDefinition(RequestEntry.ItemId);
			if (ItemDefinition.IsNull())
			{
				UE_LOG(LogModularGameplayAbilities, Warning, TEXT("配装决议：标识 [%s] 未在物品目录中登记，该条请求被丢弃。"), *RequestEntry.ItemId.ToString());
				continue;
			}

			// 同一槽位只接受一条覆盖，重复请求以先到者为准。
			bool bSlotAlreadyOverridden = false;
			OverriddenSlots.Add(RequestEntry.SlotIndex, &bSlotAlreadyOverridden);
			if (bSlotAlreadyOverridden)
			{
				UE_LOG(LogModularGameplayAbilities, Warning, TEXT("配装决议：槽位 %d 被重复请求，后一条被丢弃。"), RequestEntry.SlotIndex);
				continue;
			}

			FResolvedLoadoutEntry OverrideEntry;
			OverrideEntry.ItemDefinition = ItemDefinition;
			OverrideEntry.StackCount     = 1;
			OverrideEntry.QuickBarSlot   = RequestEntry.SlotIndex;

			MergeEntry(Result, OverrideEntry);
		}

		return Result;
	}
}
