// 物品展示查询子系统实现。

#include "ItemDisplay/EquipmentDisplaySubsystem.h"

#include "ItemDisplay/ItemVisualData.h"
#include "Algo/StableSort.h"
#include "DataRegistrySubsystem.h"
#include "DataRegistryTypes.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Inventory/InventoryItemInstance.h"

template <typename RowType>
const RowType* UEquipmentDisplaySubsystem::GetCachedItem(const FDataRegistryId& ItemId) const
{
	const UDataRegistrySubsystem* Registry = UDataRegistrySubsystem::Get();
	return Registry != nullptr ? Registry->GetCachedItem<RowType>(ItemId) : nullptr;
}

bool UEquipmentDisplaySubsystem::ResolveDisplayId(UInventoryItemInstance* Item, FDataRegistryId& OutId) const
{
	if (Item == nullptr)
	{
		return false;
	}

	const UInventoryFragment_ItemDisplay* Fragment = Item->FindFragmentByClass<UInventoryFragment_ItemDisplay>();
	if (Fragment == nullptr || !Fragment->DisplayId.IsValid())
	{
		return false;
	}

	OutId = Fragment->DisplayId;
	return true;
}

void UEquipmentDisplaySubsystem::SortItemsByDisplayOrder(FDataRegistryType RegistryType, TArray<UInventoryItemInstance*>& Items) const
{
	TArray<FDataRegistryId> OrderedIds;
	UDataRegistrySubsystem::GetPossibleDataRegistryIdList(RegistryType, OrderedIds);
	if (OrderedIds.IsEmpty())
	{
		return;
	}

	TMap<FName, int32> RankByName;
	RankByName.Reserve(OrderedIds.Num());
	for (int32 Rank = 0; Rank < OrderedIds.Num(); ++Rank)
	{
		RankByName.Add(OrderedIds[Rank].ItemName, Rank);
	}

	auto GetRank = [this, &RankByName](UInventoryItemInstance* Item) -> int32
	{
		FDataRegistryId Id;
		const int32* Rank = ResolveDisplayId(Item, Id) ? RankByName.Find(Id.ItemName) : nullptr;
		return Rank != nullptr ? *Rank : MAX_int32;
	};

	// 稳定排序：表外物品全部落在末尾且保持原有相对顺序
	Algo::StableSortBy(Items, GetRank, TLess<int32>());
}

bool UEquipmentDisplaySubsystem::GetDisplayForItem(UInventoryItemInstance* Item, FItemDisplayView& OutView)
{
	FDataRegistryId Id;
	if (!ResolveDisplayId(Item, Id))
	{
		return false;
	}

	FDisplayCacheEntry& Entry = DisplayCache.FindOrAdd(Id);
	if (Entry.State == EDisplayLoadState::None)
	{
		BeginAcquire(Id);
	}

	// 引用可能因 BeginAcquire 内部同步命中而写入，故重新取一次。
	const FDisplayCacheEntry& Current = DisplayCache.FindChecked(Id);

	OutView.DisplayName = Current.DisplayName;
	OutView.Description = Current.Description;
	OutView.TintColor = Current.TintColor;
	OutView.Icon = Current.Icon.Get();
	OutView.bReady = (Current.State == EDisplayLoadState::Ready);

	// 行数据可用（含正在加载图标）即返回 true，让 UI 先显示名称。
	return Current.State == EDisplayLoadState::AssetPending
		|| Current.State == EDisplayLoadState::Ready;
}

void UEquipmentDisplaySubsystem::PrimeDisplayForItem(UInventoryItemInstance* Item)
{
	FDataRegistryId Id;
	if (!ResolveDisplayId(Item, Id))
	{
		return;
	}

	FDisplayCacheEntry& Entry = DisplayCache.FindOrAdd(Id);
	if (Entry.State == EDisplayLoadState::None)
	{
		BeginAcquire(Id);
	}
}

void UEquipmentDisplaySubsystem::BeginAcquire(const FDataRegistryId& ItemId)
{
	// 快路径：注册表行已在缓存中，直接同步处理。
	if (const FItemDisplayData* Row = GetCachedItem<FItemDisplayData>(ItemId))
	{
		OnRowReady(ItemId, *Row);
		return;
	}

	UDataRegistrySubsystem* Registry = UDataRegistrySubsystem::Get();
	if (Registry == nullptr)
	{
		DisplayCache.FindOrAdd(ItemId).State = EDisplayLoadState::Failed;
		return;
	}

	DisplayCache.FindOrAdd(ItemId).State = EDisplayLoadState::RowPending;
	Registry->AcquireItem(ItemId, FDataRegistryItemAcquiredCallback::CreateUObject(this, &UEquipmentDisplaySubsystem::OnRowAcquired));
}

void UEquipmentDisplaySubsystem::OnRowAcquired(const FDataRegistryAcquireResult& Result)
{
	if (const FItemDisplayData* Row = Result.GetItem<FItemDisplayData>())
	{
		OnRowReady(Result.ItemId, *Row);
		return;
	}

	// 未取到行：标记失败，避免反复重试。
	if (FDisplayCacheEntry* Entry = DisplayCache.Find(Result.ItemId))
	{
		Entry->State = EDisplayLoadState::Failed;
	}
}

void UEquipmentDisplaySubsystem::OnRowReady(const FDataRegistryId& ItemId, const FItemDisplayData& Row)
{
	FDisplayCacheEntry& Entry = DisplayCache.FindOrAdd(ItemId);
	Entry.DisplayName = Row.DisplayName;
	Entry.Description = Row.Description;
	Entry.TintColor = Row.TintColor;
	Entry.VisualAssetId = Row.VisualAssetId;
	Entry.AssetBundles = Row.AssetBundles;

	// 行未指定视觉资产：仅有文字数据，直接就绪。
	if (!Row.VisualAssetId.IsValid())
	{
		Entry.State = EDisplayLoadState::Ready;
		return;
	}

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (AssetManager == nullptr)
	{
		// 无 AssetManager 时退化为文字数据可用，不再尝试加载图标。
		Entry.State = EDisplayLoadState::Ready;
		return;
	}

	Entry.State = EDisplayLoadState::AssetPending;
	Entry.AssetHandle = AssetManager->LoadPrimaryAsset(
		Row.VisualAssetId,
		Row.AssetBundles,
		FStreamableDelegate::CreateUObject(this, &UEquipmentDisplaySubsystem::OnVisualLoaded, ItemId));

	// 同步已加载时句柄可能立即完成，回调已在上面执行；此处无需额外处理。
}

void UEquipmentDisplaySubsystem::OnVisualLoaded(FDataRegistryId ItemId)
{
	FDisplayCacheEntry* Entry = DisplayCache.Find(ItemId);
	if (Entry == nullptr)
	{
		return;
	}

	if (UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
	{
		if (const UItemVisualData* Visual = AssetManager->GetPrimaryAssetObject<UItemVisualData>(Entry->VisualAssetId))
		{
			// 已请求 "UI" Bundle，Icon 软引用此时应已加载。
			Entry->Icon = Visual->Icon.Get();
		}
	}

	Entry->State = EDisplayLoadState::Ready;
}
