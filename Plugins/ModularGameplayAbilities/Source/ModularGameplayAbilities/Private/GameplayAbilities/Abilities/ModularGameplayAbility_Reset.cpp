// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayAbilities/Abilities/ModularGameplayAbility_Reset.h"

#include "ModularAbilityTags.h"
#include "ActorComponent/ModularAbilitySystemComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "ModularGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularGameplayAbility_Reset)

UModularGameplayAbility_Reset::UModularGameplayAbility_Reset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// CDO 上注册默认 GameplayEvent Trigger。
		FAbilityTriggerData TriggerData;
		TriggerData.TriggerTag = ModularGameplayTags::GameplayEvent_RequestReset;
		TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(TriggerData);
	}
}

void UModularGameplayAbility_Reset::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	check(ActorInfo);

	UModularAbilitySystemComponent* AbilityComponent = CastChecked<UModularAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());

	FGameplayTagContainer AbilityTypesToIgnore;
	AbilityTypesToIgnore.AddTag(ModularAbilityTags::Ability_Behavior_SurvivesDeath);

	// 取消其它 Ability 并阻塞新的激活；死亡豁免 Tag 由 AbilityTypesToIgnore 指定。
	AbilityComponent->CancelAbilities(nullptr, &AbilityTypesToIgnore, this);

	SetCanBeCanceled(false);

	// 在 Character 上执行 Reset。
	if (AModularCharacter* ModularChar = Cast<AModularCharacter>(CurrentActorInfo->AvatarActor.Get()))
	{
		ModularChar->Reset();
	}

	// 广播 GameplayMessage，通知监听方。
	FModularPlayerResetMessage Message;
	Message.OwnerPlayerState = CurrentActorInfo->OwnerActor.Get();
	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(this);
	MessageSystem.BroadcastMessage(ModularGameplayTags::GameplayEvent_Reset, Message);

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	constexpr bool bReplicateEndAbility = true;
	constexpr bool bWasCanceled = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCanceled);
}
