// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayAbilities/ModularGameplayAbility.h"

#include "GameplayAbility_Pickup.generated.h"

#define UE_API EQUIPMANAGER_API

struct FGameplayAbilityActorInfo;
struct FGameplayEventData;

/**
 * 拾取能力。
 *
 * 由交互系统通过 GameplayEvent 触发，将事件 Target（实现 IPickupable 的地上物）
 * 注入拥有者控制器上的背包组件。仅在服务器执行。
 */
UCLASS(MinimalAPI)
class UGameplayAbility_Pickup : public UModularGameplayAbility
{
	GENERATED_BODY()

public:
	UE_API UGameplayAbility_Pickup(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UGameplayAbility Interface
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~End UGameplayAbility Interface
};

#undef UE_API
