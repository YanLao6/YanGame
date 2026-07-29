// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/YanGameplayAbility.h"

UYanGameplayAbility::UYanGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

void UYanGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 实例可被复用，且上次结束若未走到有效分支会残留标记，激活时一律重置
	bCooldownPendingOnEnd = false;

	// 只提交 cost；冷却推迟到 EndAbility，使子类得以在 ActivateAbility 中拒绝激活
	if (!CommitAbilityCost(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 子类若在 ActivateAbility 内自行结束，此处已非 active，本次激活不计冷却
	if (IsActive())
	{
		bCooldownPendingOnEnd = true;
	}
}

void UYanGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bCooldownPendingOnEnd && IsEndAbilityValid(Handle, ActorInfo))
	{
		bCooldownPendingOnEnd = false;
		ApplyCooldown(Handle, ActorInfo, ActivationInfo);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UYanGameplayAbility::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	K2_InputReleased();
}

void UYanGameplayAbility::K2_InputReleased_Implementation() {}
