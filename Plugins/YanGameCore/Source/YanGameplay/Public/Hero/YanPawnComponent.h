// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActorComponent/ModularPawnComponent.h"
#include "YanPawnComponent.generated.h"

#define UE_API YANGAMEPLAY_API


UCLASS(MinimalAPI, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UYanPawnComponent : public UModularPawnComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UE_API UYanPawnComponent(const FObjectInitializer& ObjectInitializer);

protected:
	UE_API virtual void BeginPlay() override;
};

#undef UE_API
