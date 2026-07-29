// 物品展示数据、桥接片段与展示视图声明。

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "DataRegistryId.h"
#include "UObject/PrimaryAssetId.h"
#include "Inventory/InventoryItemDefinition.h"
#include "ItemDisplayTypes.generated.h"

class UTexture2D;

/**
 * 物品展示数据行。
 *
 * 注册进 DataRegistry（类型 ItemDisplay），以 FDataRegistryId 寻址。
 * 行内不直接放重资源，而是用 PrimaryAssetId 指向视觉数据资产，交由 AssetManager 按 Bundle 流送。
 */
USTRUCT(BlueprintType)
struct FItemDisplayData : public FTableRowBase
{
	GENERATED_BODY()

	/** 视觉数据资产（UItemVisualData）的主资产 Id。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Display, meta = (AllowedTypes = "ItemVisual"))
	FPrimaryAssetId VisualAssetId;

	/** 加载该视觉资产时请求的 AssetBundle，需与资产项上 meta=(AssetBundles=...) 对应。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Display)
	TArray<FName> AssetBundles;

	/** 物品在界面上显示的名称。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Display)
	FText DisplayName;

	/** 物品的详细描述。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Display, meta = (MultiLine = true))
	FText Description;

	/** 槽位底色或描边色，供 UI 表达稀有度、分类等。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Display)
	FLinearColor TintColor = FLinearColor::White;
};

/**
 * 物品展示片段。
 *
 * 挂在物品定义上，把物品逻辑桥接到展示注册表，并声明其快捷栏归属类别。
 * 未挂本片段（或 Id 无效）的物品不会进入快捷栏/背包 UI。
 */
UCLASS(meta = (DisplayName = "Item Display"))
class EQUIPMANAGER_API UInventoryFragment_ItemDisplay : public UInventoryItemFragment
{
	GENERATED_BODY()

public:
	/** 该物品对应的展示注册表 Id（如 ItemDisplay:HealthPotion）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Display)
	FDataRegistryId DisplayId;

	/** 该物品所属的快捷栏类别，决定它能被放入哪些配置了准入规则的槽位。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Display, meta = (Categories = "EM.QuickBar.Category"))
	FGameplayTagContainer SlotCategoryTags;
};

/**
 * 物品展示视图。
 *
 * 展示子系统返回给 UI 的解析结果：名称/描述随行数据即时可得，
 * 图标随 Bundle 异步加载完成后才非空，bReady 表示图标资源已就绪。
 */
USTRUCT(BlueprintType)
struct FItemDisplayView
{
	GENERATED_BODY()

	/** 物品显示名称。 */
	UPROPERTY(BlueprintReadOnly, Category = Display)
	FText DisplayName;

	/** 物品描述。 */
	UPROPERTY(BlueprintReadOnly, Category = Display)
	FText Description;

	/** 已加载的物品图标；异步加载完成前为 null。 */
	UPROPERTY(BlueprintReadOnly, Category = Display)
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** 槽位底色或描边色。 */
	UPROPERTY(BlueprintReadOnly, Category = Display)
	FLinearColor TintColor = FLinearColor::White;

	/** 图标资源是否已加载完成。 */
	UPROPERTY(BlueprintReadOnly, Category = Display)
	bool bReady = false;
};
