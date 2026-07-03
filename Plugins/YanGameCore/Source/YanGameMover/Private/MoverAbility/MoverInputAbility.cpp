#include "MoverAbility/MoverInputAbility.h"

#include "MoverComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverInputAbility)

void UMoverInputAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// Commit 必须先于 MoverComp 注册；失败时直接取消
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	if (UMoverComponent* MoverComp = ActorInfo->AvatarActor->FindComponentByClass<UMoverComponent>())
	{
		CachedMoverComp = MoverComp;
		CachedMoverComp->InputProducers.AddUnique(this);
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UMoverInputAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UMoverInputAbility::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);

	K2_InputPressed();
}

void UMoverInputAbility::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	K2_InputReleased();
}

void UMoverInputAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
}

void UMoverInputAbility::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnRemoveAbility(ActorInfo, Spec);

	if (CachedMoverComp)
	{
		CachedMoverComp->InputProducers.Remove(this);
		CachedMoverComp = nullptr;
	}
}

void UMoverInputAbility::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	ProduceMoverInput(CachedMoverComp, SimTimeMs, InputCmdResult);
}

void UMoverInputAbility::K2_InputPressed_Implementation() {}
void UMoverInputAbility::K2_InputReleased_Implementation() {}
void UMoverInputAbility::ProduceMoverInput_Implementation(UMoverComponent* MoverComp, int32 SimTimeMs, FMoverInputCmdContext& InputCmd) {}
