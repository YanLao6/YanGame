// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonLocalPlayer.h"
#include "YanLocalPlayer.generated.h"

#define UE_API YANGAMECORE_API

/**
 * 
 */
UCLASS(MinimalAPI)
class UYanLocalPlayer : public UCommonLocalPlayer
{
	GENERATED_BODY()
	
public:
	UE_API UYanLocalPlayer();
};

#undef UE_API
