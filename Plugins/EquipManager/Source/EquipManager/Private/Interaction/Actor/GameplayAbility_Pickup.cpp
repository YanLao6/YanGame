// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/Actor/GameplayAbility_Pickup.h"

#include "GameFramework/Controller.h"
#include "Inventory/IPickupable.h"
#include "Inventory/InventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayAbility_Pickup)

UGameplayAbility_Pickup::UGameplayAbility_Pickup(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 拾取会写背包（AddPickupToInventory 为 BlueprintAuthorityOnly），须在服务器执行
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UGameplayAbility_Pickup::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		const AActor* TargetActor = TriggerEventData ? ToRawPtr(TriggerEventData->Target) : nullptr;
		AController* Controller = GetControllerFromActorInfo();

		if (TargetActor && Controller)
		{
			if (UInventoryManagerComponent* InventoryComponent = Controller->FindComponentByClass<UInventoryManagerComponent>())
			{
				TScriptInterface<IPickupable> Pickupable = UPickupableStatics::GetFirstPickupableFromActor(TargetActor);
				if (Pickupable)
				{
					UPickupableStatics::AddPickupToInventory(InventoryComponent, Pickupable);
					// 如需拾取后移除地上物，可在此销毁 TargetActor 或走刷新冷却逻辑。
				}
			}
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
}
