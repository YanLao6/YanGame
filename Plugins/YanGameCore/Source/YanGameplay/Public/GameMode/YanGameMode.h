// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameMode/ModularExperienceGameMode.h"

#include "YanGameMode.generated.h"

#define UE_API YANGAMEPLAY_API

/**
 * 
 */
UCLASS(MinimalAPI)
class AYanGameMode : public AModularExperienceGameMode
{
	GENERATED_BODY()

public:
	UE_API explicit AYanGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin AModularExperienceGameMode Interface
	UE_API virtual const UModularPawnData* GetPawnDataForController(const AController* InController) const override;
	//~End AModularExperienceGameMode Interface
};

#undef UE_API
