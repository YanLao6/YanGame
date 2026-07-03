// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActorComponent/ModularInputConfigComponent.h"
#include "YanInputComponent.generated.h"


/**
 * 
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class YANGAMECORE_API UYanInputComponent : public UModularInputConfigComponent
{
	GENERATED_BODY()

public:
	UYanInputComponent(const FObjectInitializer& ObjectInitializer);

};
