// 物品视觉数据资产声明。

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemVisualData.generated.h"

#define UE_API EQUIPMANAGER_API

class UTexture2D;

/**
 * 物品视觉数据资产。
 *
 * 以 PrimaryDataAsset 承载物品的重美术资源，由 AssetManager 按 AssetBundle 异步流送。
 * 快捷栏/背包只在物品可见时加载其 UI Bundle，避免一次性把全部图标常驻内存。
 */
UCLASS(MinimalAPI, BlueprintType)
class UItemVisualData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 物品图标，归入 "UI" Bundle，仅在界面需要时按需加载。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Display, meta = (AssetBundles = "UI"))
	TSoftObjectPtr<UTexture2D> Icon;

	//~Begin UPrimaryDataAsset Interface
	// 固定主资产类型为 ItemVisual，便于 AssetManager 统一扫描与按 Id 加载。
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FPrimaryAssetType(TEXT("ItemVisual")), GetFName());
	}
	//~End UPrimaryDataAsset Interface
};

#undef UE_API
