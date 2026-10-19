#include "System/ColorTransferGameData.h"

#include "DataAsset/ModularAssetManager.h"
#include "System/ColorTransferSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorTransferGameData)

const UColorTransferGameData& UColorTransferGameData::Get()
{
	return UModularAssetManager::Get().GetOrLoadTypedGameData<UColorTransferGameData>(GetDefault<UColorTransferSettings>()->GameDataPath);
}
