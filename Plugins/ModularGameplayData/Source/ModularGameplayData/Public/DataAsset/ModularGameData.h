// Copyright Chronicler.


#pragma once

#include "Engine/DataAsset.h"
#include "ModularGameData.generated.h"

#define UE_API MODULARGAMEPLAYDATA_API

/**
 * 全局游戏数据 Primary Data Asset（Const，运行时不可改）。
 */
UCLASS(MinimalAPI, BlueprintType, Const, Meta = (DisplayName = "Modular Game Data", ShortTooltip = "Data asset containing global game data."))
class UModularGameData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UE_API UModularGameData();

	/** 经 UModularAssetManager 获取已加载的全局 GameData。 */
	static UE_API const UModularGameData& Get();

};

#undef UE_API
