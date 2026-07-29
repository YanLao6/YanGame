// 物品展示查询子系统声明。

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DataRegistryId.h"
#include "UObject/PrimaryAssetId.h"
#include "ItemDisplay/ItemDisplayTypes.h"
#include "EquipmentDisplaySubsystem.generated.h"

class UInventoryItemInstance;
class UTexture2D;
struct FStreamableHandle;
struct FDataRegistryAcquireResult;
struct FDataRegistryId;

/** 单个展示项的加载阶段。 */
enum class EDisplayLoadState : uint8
{
	None,          // 尚未发起
	RowPending,    // 正在异步 Acquire 注册表行
	AssetPending,  // 行已就绪，正在异步流送视觉资产
	Ready,         // 行与图标均已就绪（无图标资产时也视为 Ready）
	Failed         // 行不存在或获取失败
};

/** 展示项缓存条目，聚合行数据、加载状态与图标句柄。 */
struct FDisplayCacheEntry
{
	EDisplayLoadState State = EDisplayLoadState::None;

	FText DisplayName;
	FText Description;
	FLinearColor TintColor = FLinearColor::White;

	FPrimaryAssetId VisualAssetId;
	TArray<FName> AssetBundles;

	// 图标由 AssetManager 保活（LoadPrimaryAsset 引用计数），此处弱引用即可
	TWeakObjectPtr<UTexture2D> Icon;
	// 保留加载句柄以持有资产生命周期
	TSharedPtr<FStreamableHandle> AssetHandle;
};

/**
 * 物品展示查询子系统。
 *
 * 封装引擎 DataRegistry 与 AssetManager：以物品上的 FDataRegistryId 为键，
 * 异步取展示行、再按 PrimaryAssetId + Bundle 流送图标，结果落缓存。
 * 对 UI 暴露同步的按物品查询接口，配合轮询即可：首帧触发异步、后续帧命中缓存。
 */
UCLASS()
class EQUIPMANAGER_API UEquipmentDisplaySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * 取物品的展示视图；返回 true 表示行数据已就绪（名称/描述可用），
	 * 图标可能仍在异步加载（OutView.Icon 为 null、bReady 为 false），后续调用会补齐。
	 * 首次对某物品调用会触发异步加载。
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemDisplay")
	bool GetDisplayForItem(UInventoryItemInstance* Item, FItemDisplayView& OutView);

	/** 主动预热某物品的展示数据与图标，供打开界面前批量预加载。 */
	UFUNCTION(BlueprintCallable, Category = "ItemDisplay")
	void PrimeDisplayForItem(UInventoryItemInstance* Item);

	/**
	 * 按展示注册表的数据源顺序（即 DataTable 行顺序）稳定排序，使界面呈现顺序由策划表决定而非获得先后。
	 * 不属于该类型或无展示片段的物品排到末尾，相互间保持原有相对顺序。
	 *
	 * 顺序来自数据源已加载的行名，因此该注册表的 DataTable 需已在内存中：
	 * 编辑器下会自动加载，打包运行时须在其 DataTable Source 上启用 bPrecacheTable，
	 * 否则顺序不可得，此时保持传入顺序不变。
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemDisplay")
	void SortItemsByDisplayOrder(FDataRegistryType RegistryType, UPARAM(ref) TArray<UInventoryItemInstance*>& Items) const;

	/** 类型安全的同步缓存读取，未缓存时返回 nullptr（C++ 内部便捷接口）。 */
	template <typename RowType>
	const RowType* GetCachedItem(const FDataRegistryId& ItemId) const;

private:
	// 解析物品上的展示片段，取出 FDataRegistryId。
	bool ResolveDisplayId(UInventoryItemInstance* Item, FDataRegistryId& OutId) const;

	// 对某 Id 发起加载：优先同步缓存命中，否则异步 Acquire。
	void BeginAcquire(const FDataRegistryId& ItemId);

	// 注册表行就绪：写入行数据并按需发起图标流送。
	void OnRowReady(const FDataRegistryId& ItemId, const FItemDisplayData& Row);

	// 异步 Acquire 完成回调。
	void OnRowAcquired(const FDataRegistryAcquireResult& Result);

	// 视觉资产 Bundle 加载完成回调。
	void OnVisualLoaded(FDataRegistryId ItemId);

private:
	// 以展示 Id 为键的加载缓存。
	TMap<FDataRegistryId, FDisplayCacheEntry> DisplayCache;
};
