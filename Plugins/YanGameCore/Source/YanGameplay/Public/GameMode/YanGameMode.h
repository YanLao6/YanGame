// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameMode/ModularExperienceGameMode.h"

#include "YanGameMode.generated.h"

/**
 * 
 */
UCLASS()
class YANGAMEPLAY_API AYanGameMode : public AModularExperienceGameMode
{
	GENERATED_BODY()

public:
	explicit AYanGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
