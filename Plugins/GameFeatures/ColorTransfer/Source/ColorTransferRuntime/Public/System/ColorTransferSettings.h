#pragma once

#include "Engine/DeveloperSettings.h"

#include "ColorTransferSettings.generated.h"

class UColorTransferGameData;

/** 颜料玩法插件的项目设置。 */
UCLASS(MinimalAPI, Config = Game, DefaultConfig, Meta = (DisplayName = "Color Transfer"))
class UColorTransferSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 全局颜料数据资产，见 UColorTransferGameData。 */
	UPROPERTY(Config, EditAnywhere, Category = "Yan|ColorTransfer")
	TSoftObjectPtr<UColorTransferGameData> GameDataPath;
};
