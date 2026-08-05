// Copyright Chronicler.

#pragma once

#include "DataAsset/ModularAbilityData.h"
#include "Engine/DeveloperSettings.h"

#include "ModularGameplayAbilitiesConfig.generated.h"

/**
 * DeveloperSettings：指向全局 UModularAbilityData 软引用路径。
 */
UCLASS(MinimalAPI, Config = Game)
class UModularGameplayAbilitiesConfig : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 构造默认配置对象。 */
	UModularGameplayAbilitiesConfig();


	// 全局 ModularAbilityData 资产路径（Config）。
	UPROPERTY(Config)
	TSoftObjectPtr<UModularAbilityData> ModularAbilityDataPath;
};
