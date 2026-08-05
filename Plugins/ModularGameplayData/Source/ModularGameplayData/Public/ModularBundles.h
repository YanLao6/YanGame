// Copyright Chronicler.

#pragma once

#include "ModularBundles.generated.h"

#define UE_API MODULARGAMEPLAYDATA_API

/** PrimaryAsset Bundle 名称常量（与 AssetManager bundle 命名对齐）。 */
USTRUCT()
struct FModularBundles
{
	GENERATED_BODY()

	static UE_API const FName Equipped;
};

#undef UE_API
