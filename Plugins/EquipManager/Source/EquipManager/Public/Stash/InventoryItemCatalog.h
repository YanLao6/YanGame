// 物品目录声明。

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"
#include "InventoryItemCatalog.generated.h"

#define UE_API EQUIPMANAGER_API

class UInventoryItemDefinition;

/** 目录中的一条登记：标识与物品定义类的对应关系。 */
USTRUCT(BlueprintType)
struct FInventoryCatalogEntry
{
	GENERATED_BODY()

	/** 物品的稳定标识；一经发布不得改动或复用，否则存档中的旧记录会指向别的物品。 */
	UPROPERTY(EditDefaultsOnly, Category=Items)
	FName ItemId;

	/**
	 * 对应的物品定义类。
	 *
	 * 用软引用登记：物品定义多半随 GameFeature 插件一同分发，
	 * 硬引用会迫使目录在启用该插件前就把它们全部拉起来。
	 */
	UPROPERTY(EditDefaultsOnly, Category=Items)
	TSoftClassPtr<UInventoryItemDefinition> ItemDefinition;
};

/**
 * 物品目录。
 *
 * 存档与网络传输中的 ItemId 全靠本资产解析成物品定义类，是仓库数据的唯一解释者。
 * 标识与定义类的对应只在此处维护，物品定义本身不持有标识——放两份必然会不一致。
 */
UCLASS(MinimalAPI, BlueprintType)
class UInventoryItemCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 按标识取出物品定义类并完成加载，未登记或加载失败时返回空。 */
	UE_API TSubclassOf<UInventoryItemDefinition> ResolveItemDefinition(FName ItemId) const;

	/** 按标识取出尚未加载的物品定义类引用，未登记时返回空引用。 */
	UE_API TSoftClassPtr<UInventoryItemDefinition> FindItemDefinition(FName ItemId) const;

	/** 反查某个物品定义类的标识，未登记时返回 NAME_None。 */
	UE_API FName FindItemId(TSoftClassPtr<UInventoryItemDefinition> ItemDefinition) const;

	/** 判断某个标识是否已登记。 */
	UE_API bool IsItemRegistered(FName ItemId) const;

	/** 返回目录中登记的全部标识。 */
	UE_API TArray<FName> GetAllItemIds() const;

#if WITH_EDITOR
	//~Begin UObject Interface
	// 校验标识非空且互不重复，重复的标识会让解析结果取决于登记顺序。
	UE_API virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
	//~End UObject Interface
#endif

protected:
	/** 目录中的全部登记。 */
	UPROPERTY(EditDefaultsOnly, Category=Items, meta=(TitleProperty=ItemId))
	TArray<FInventoryCatalogEntry> Entries;

private:
	// 标识到条目下标的索引；首次查询时按需建立，目录资产在运行期不会改动。
	mutable TMap<FName, int32> EntryLookup;

	// 确保索引已建立。
	void EnsureLookupBuilt() const;
};

#undef UE_API
