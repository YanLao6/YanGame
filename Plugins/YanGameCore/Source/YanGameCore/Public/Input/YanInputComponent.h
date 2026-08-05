// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActorComponent/ModularInputConfigComponent.h"
#include "YanInputComponent.generated.h"

#define UE_API YANGAMECORE_API


/**
 * 
 */
UCLASS(MinimalAPI, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UYanInputComponent : public UModularInputConfigComponent
{
	GENERATED_BODY()

public:
	UE_API UYanInputComponent(const FObjectInitializer& ObjectInitializer);

};

#undef UE_API
