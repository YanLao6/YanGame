// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameMode/ModularExperienceGameState.h"

#include "YanGameState.generated.h"

#define UE_API YANGAMEPLAY_API

class UModularAbilitySystemComponent;

/**
 * 
 */
UCLASS(MinimalAPI)
class AYanGameState : public AModularExperienceGameState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	UE_API AYanGameState(const FObjectInitializer& ObjectInitializer);

	//~AActor interface
	UE_API virtual void PostInitializeComponents() override;
	//~End of AActor interface

	//~IAbilitySystemInterface
	UE_API virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~End of IAbilitySystemInterface

	UFUNCTION(BlueprintCallable, Category = "Modular|GameState")
	UModularAbilitySystemComponent* GetModularAbilitySystemComponent() const { return AbilitySystemComponent; }

private:
	// 游戏范围内的东西（主要是游戏提示）的 ability system component
	UPROPERTY(VisibleAnywhere, Category = "Modular|GameState")
	TObjectPtr<UModularAbilitySystemComponent> AbilitySystemComponent;
};

#undef UE_API
