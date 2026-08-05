// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayAbilities/ModularGameplayAbility.h"

#include "ModularGameplayAbility_Reset.generated.h"

#define UE_API MODULARGAMEPLAYABILITIES_API

class AActor;
class UObject;
struct FGameplayAbilityActorInfo;
struct FGameplayEventData;

/**
 * 用于快速把玩家重置回初始出生状态的 GameplayAbility。
 *
 * 默认通过 GameplayEvent（如 GameplayEvent.RequestReset）在 Server 上自动触发激活。
 */
UCLASS(MinimalAPI)
class UModularGameplayAbility_Reset : public UModularGameplayAbility
{
	GENERATED_BODY()

public:
	/** 构造并配置 Trigger 与网络策略。 */
	UE_API UModularGameplayAbility_Reset(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};



/** 广播重置成功时的消息载荷（GameplayMessageSubsystem）。 */
USTRUCT(BlueprintType)
struct FModularPlayerResetMessage
{
	GENERATED_BODY()

	// 发起重置的 PlayerState（OwnerActor）。
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> OwnerPlayerState = nullptr;
};

#undef UE_API
