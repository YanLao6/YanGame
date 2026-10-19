// 物品目录实现。

#include "Stash/InventoryItemCatalog.h"

#include "Inventory/InventoryItemDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(InventoryItemCatalog)

#define LOCTEXT_NAMESPACE "InventoryItemCatalog"

TSubclassOf<UInventoryItemDefinition> UInventoryItemCatalog::ResolveItemDefinition(FName ItemId) const
{
	const TSoftClassPtr<UInventoryItemDefinition> SoftDefinition = FindItemDefinition(ItemId);
	if (SoftDefinition.IsNull())
	{
		return nullptr;
	}

	// 物品定义随 GameFeature 插件分发，取用那一刻才是它确实需要在内存里的时刻。
	return SoftDefinition.LoadSynchronous();
}

TSoftClassPtr<UInventoryItemDefinition> UInventoryItemCatalog::FindItemDefinition(FName ItemId) const
{
	EnsureLookupBuilt();

	if (const int32* EntryIndex = EntryLookup.Find(ItemId))
	{
		return Entries[*EntryIndex].ItemDefinition;
	}

	return nullptr;
}

FName UInventoryItemCatalog::FindItemId(TSoftClassPtr<UInventoryItemDefinition> ItemDefinition) const
{
	if (ItemDefinition.IsNull())
	{
		return NAME_None;
	}

	// 反查只在写入仓库时发生，条目规模有限，不为此再维护一张反向表。
	for (const FInventoryCatalogEntry& Entry : Entries)
	{
		if (Entry.ItemDefinition.ToSoftObjectPath() == ItemDefinition.ToSoftObjectPath())
		{
			return Entry.ItemId;
		}
	}

	return NAME_None;
}

bool UInventoryItemCatalog::IsItemRegistered(FName ItemId) const
{
	EnsureLookupBuilt();

	return EntryLookup.Contains(ItemId);
}

TArray<FName> UInventoryItemCatalog::GetAllItemIds() const
{
	TArray<FName> Result;
	Result.Reserve(Entries.Num());

	for (const FInventoryCatalogEntry& Entry : Entries)
	{
		if (!Entry.ItemId.IsNone())
		{
			Result.Add(Entry.ItemId);
		}
	}

	return Result;
}

void UInventoryItemCatalog::EnsureLookupBuilt() const
{
	if (EntryLookup.Num() == Entries.Num())
	{
		return;
	}

	EntryLookup.Reset();
	EntryLookup.Reserve(Entries.Num());

	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		const FName ItemId = Entries[EntryIndex].ItemId;
		if (!ItemId.IsNone())
		{
			// 重复标识由资产校验拦截，运行期以首条为准，保证解析结果稳定。
			EntryLookup.FindOrAdd(ItemId, EntryIndex);
		}
	}
}

#if WITH_EDITOR
//~Begin UObject Interface
EDataValidationResult UInventoryItemCatalog::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	TSet<FName> UsedIds;
	int32       EntryIndex = 0;

	for (const FInventoryCatalogEntry& Entry : Entries)
	{
		if (Entry.ItemId.IsNone())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("EmptyItemId", "物品目录的第 {0} 条登记没有标识，存档将无法指代该物品。"), FText::AsNumber(EntryIndex)));
		}
		else
		{
			bool bAlreadyUsed = false;
			UsedIds.Add(Entry.ItemId, &bAlreadyUsed);
			if (bAlreadyUsed)
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(FText::Format(LOCTEXT("DuplicateItemId", "物品目录中标识 [{0}] 被重复登记，解析结果将取决于登记顺序。"), FText::FromName(Entry.ItemId)));
			}
		}

		if (Entry.ItemDefinition.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("NullItemDefinition", "物品目录中标识 [{0}] 未指定物品定义。"), FText::FromName(Entry.ItemId)));
		}

		++EntryIndex;
	}

	return Result;
}
//~End UObject Interface
#endif

#undef LOCTEXT_NAMESPACE
