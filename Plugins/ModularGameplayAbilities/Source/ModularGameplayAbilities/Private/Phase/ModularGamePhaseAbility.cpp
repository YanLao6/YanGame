// Fill out your copyright notice in the Description page of Project Settings.


#include "Phase/ModularGamePhaseAbility.h"

#include "AbilitySystemComponent.h"
#include "Misc/DataValidation.h"
#include "Phase/ModularGamePhaseSubsystem.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularGamePhaseAbility)

#define LOCTEXT_NAMESPACE "UModularGamePhaseAbility"

UModularGamePhaseAbility::UModularGamePhaseAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ReplicationPolicy  = EGameplayAbilityReplicationPolicy::ReplicateNo;
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	NetSecurityPolicy  = EGameplayAbilityNetSecurityPolicy::ServerOnly;
}

void UModularGamePhaseAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (ActorInfo->IsNetAuthority())
	{
		UWorld*                     World          = ActorInfo->AbilitySystemComponent->GetWorld();
		UModularGamePhaseSubsystem* PhaseSubsystem = UWorld::GetSubsystem<UModularGamePhaseSubsystem>(World);
		PhaseSubsystem->OnBeginPhase(this, Handle);
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UModularGamePhaseAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ActorInfo->IsNetAuthority())
	{
		UWorld*                     World          = ActorInfo->AbilitySystemComponent->GetWorld();
		UModularGamePhaseSubsystem* PhaseSubsystem = UWorld::GetSubsystem<UModularGamePhaseSubsystem>(World);
		PhaseSubsystem->OnEndPhase(this, Handle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

#if WITH_EDITOR
EDataValidationResult UModularGamePhaseAbility::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	if (!GamePhaseTag.IsValid())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(LOCTEXT("GamePhaseTagNotSet", "GamePhaseTag must be set to a tag representing the current phase."));
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
