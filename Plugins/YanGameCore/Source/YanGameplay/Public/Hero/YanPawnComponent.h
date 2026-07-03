// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActorComponent/ModularPawnComponent.h"
#include "YanPawnComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class YANGAMEPLAY_API UYanPawnComponent : public UModularPawnComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UYanPawnComponent(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
};
