// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/ModularGameplayHUD.h"

#include "YanGameplayHUD.generated.h"

#define UE_API YANGAMEUI_API

/**
 * 
 */
UCLASS(MinimalAPI)
class AYanGameplayHUD : public AModularGameplayHUD
{
	GENERATED_BODY()
};

#undef UE_API
