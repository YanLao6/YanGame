#include "Abilities/YanBlockAbility.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanBlockAbility)

UE_DEFINE_GAMEPLAY_TAG(Status_Blocking, "Status.Blocking");

UYanBlockAbility::UYanBlockAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 姿态须在本地立即生效，否则玩家按下与画面反馈之间会隔一个 RTT
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	ActivationOwnedTags.AddTag(Status_Blocking);
}

void UYanBlockAbility::K2_InputReleased_Implementation()
{
	K2_EndAbility();
}
