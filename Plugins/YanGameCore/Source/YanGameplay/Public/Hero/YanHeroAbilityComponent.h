// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActorComponent/ModularAbilityExtensionComponent.h"
#include "YanHeroAbilityComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class YANGAMEPLAY_API UYanHeroAbilityComponent : public UModularAbilityExtensionComponent
{
	GENERATED_BODY()

public:
	UYanHeroAbilityComponent(const FObjectInitializer& ObjectInitializer);
};
