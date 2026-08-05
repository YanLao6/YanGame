// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActorComponent/ModularAbilityExtensionComponent.h"
#include "YanHeroAbilityComponent.generated.h"

#define UE_API YANGAMEPLAY_API


UCLASS(MinimalAPI, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UYanHeroAbilityComponent : public UModularAbilityExtensionComponent
{
	GENERATED_BODY()

public:
	UE_API UYanHeroAbilityComponent(const FObjectInitializer& ObjectInitializer);
};

#undef UE_API
