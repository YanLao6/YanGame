// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataAsset/ModularAssetManager.h"
#include "YanAssetManager.generated.h"

#define UE_API YANGAMECORE_API

/**
 * 
 */
UCLASS(MinimalAPI)
class UYanAssetManager : public UModularAssetManager
{
	GENERATED_BODY()
};

#undef UE_API
