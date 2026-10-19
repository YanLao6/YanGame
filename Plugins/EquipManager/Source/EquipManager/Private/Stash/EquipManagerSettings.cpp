// 装备与仓库模块设置实现。

#include "Stash/EquipManagerSettings.h"

#include "Stash/InventoryItemCatalog.h"
#include "Stash/LocalStashStorageProvider.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipManagerSettings)

const UInventoryItemCatalog* UEquipManagerSettings::GetItemCatalog()
{
	const UEquipManagerSettings* Settings = GetDefault<UEquipManagerSettings>();
	if (Settings->ItemCatalog.IsNull())
	{
		return nullptr;
	}

	// 目录是解析仓库数据的前提，取用时同步加载而不等异步流程。
	return Settings->ItemCatalog.LoadSynchronous();
}

TSubclassOf<UStashStorageProvider> UEquipManagerSettings::GetStorageProviderClass()
{
	const UEquipManagerSettings* Settings = GetDefault<UEquipManagerSettings>();
	if (!Settings->StorageProviderClass.IsNull())
	{
		if (UClass* ConfiguredClass = Settings->StorageProviderClass.LoadSynchronous())
		{
			return ConfiguredClass;
		}
	}

	return ULocalStashStorageProvider::StaticClass();
}
