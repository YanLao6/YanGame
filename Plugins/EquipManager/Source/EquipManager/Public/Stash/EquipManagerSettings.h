// 装备与仓库模块设置声明。

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EquipManagerSettings.generated.h"

#define UE_API EQUIPMANAGER_API

class UInventoryItemCatalog;
class UStashStorageProvider;

/**
 * 装备与仓库模块设置。
 *
 * 仓库需要知道用哪份物品目录解析标识、用哪种存储读写数据，两者都不该在代码里写死路径。
 */
UCLASS(MinimalAPI, config=Game, defaultconfig, meta=(DisplayName="Equip Manager"))
class UEquipManagerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 取出物品目录并完成加载，未配置或加载失败时返回空。 */
	static UE_API const UInventoryItemCatalog* GetItemCatalog();

	/** 取出仓库存储实现类，未配置时返回本地存档实现。 */
	static UE_API TSubclassOf<UStashStorageProvider> GetStorageProviderClass();

	/** 物品目录资产：存档与网络传输中的物品标识全靠它解析成定义类。 */
	UPROPERTY(config, EditAnywhere, Category=Stash)
	TSoftObjectPtr<UInventoryItemCatalog> ItemCatalog;

	/** 仓库存储实现；留空时使用本地存档。 */
	UPROPERTY(config, EditAnywhere, Category=Stash)
	TSoftClassPtr<UStashStorageProvider> StorageProviderClass;
};

#undef UE_API
