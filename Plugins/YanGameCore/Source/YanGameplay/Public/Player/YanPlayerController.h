// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Player/ModularAbilityPlayerController.h"
#include "YanPlayerController.generated.h"

#define UE_API YANGAMEPLAY_API

/**
 * 
 */
UCLASS(MinimalAPI)
class AYanPlayerController : public AModularAbilityPlayerController
{
	GENERATED_BODY()
};

#undef UE_API
