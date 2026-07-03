// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilities/ModularGameplayAbility.h"

#include "YanGameplayAbility.generated.h"

/**
 * 
 */
UCLASS()
class YANGAMEABILITY_API UYanGameplayAbility : public UModularGameplayAbility
{
	GENERATED_BODY()

public:
	UYanGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

	UFUNCTION(BlueprintImplementableEvent)
	void K2_OnInputReleased();

	UFUNCTION(BlueprintImplementableEvent)
	void K2_OnInputPressed();

	UFUNCTION(BlueprintImplementableEvent)
	void K2_OnActivateAbility();

	UFUNCTION(BlueprintNativeEvent)
	bool K2_OnCanActivateAbility() const;
};
