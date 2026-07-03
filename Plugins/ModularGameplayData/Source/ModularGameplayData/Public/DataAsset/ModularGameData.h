// Copyright Chronicler.


#pragma once

#include "Engine/DataAsset.h"
#include "ModularGameData.generated.h"

/**
 * 全局游戏数据 Primary Data Asset（Const，运行时不可改）。
 */
UCLASS(BlueprintType, Const, Meta = (DisplayName = "Modular Game Data", ShortTooltip = "Data asset containing global game data."))
class MODULARGAMEPLAYDATA_API UModularGameData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UModularGameData();

	/** 经 UModularAssetManager 获取已加载的全局 GameData。 */
	static const UModularGameData& Get();

};
