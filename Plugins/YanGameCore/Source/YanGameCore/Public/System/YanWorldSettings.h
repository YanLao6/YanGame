// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameMode/ModularWorldSettings.h"
#include "YanWorldSettings.generated.h"

#define UE_API YANGAMECORE_API

/**
 * 
 */
UCLASS(MinimalAPI)
class AYanWorldSettings : public AModularWorldSettings
{
	GENERATED_BODY()
};

#undef UE_API
