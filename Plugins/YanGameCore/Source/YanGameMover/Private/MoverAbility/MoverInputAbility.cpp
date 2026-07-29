#include "MoverAbility/MoverInputAbility.h"

#include "MoverComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverInputAbility)

void UMoverInputAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// InstancedPerExecution 等策略下实例晚于 OnAvatarSet 建立，缓存在此兜底；
	// 须早于 Super——子类在 ActivateAbility 中依赖 CachedMoverComp 判断移动状态
	CacheMoverComponent(ActorInfo);

	if (CachedMoverComp)
	{
		CachedMoverComp->InputProducers.AddUnique(this);
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UMoverInputAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);

	CacheMoverComponent(ActorInfo);
}

void UMoverInputAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	// Avatar 已就绪时（如运行中授予）不会再触发 OnAvatarSet，在此补一次
	CacheMoverComponent(ActorInfo);
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

void UMoverInputAbility::CacheMoverComponent(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!IsInstantiated() || CachedMoverComp || !ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		return;
	}

	CachedMoverComp = ActorInfo->AvatarActor->FindComponentByClass<UMoverComponent>();
}

void UMoverInputAbility::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	ProduceMoverInput(CachedMoverComp, SimTimeMs, InputCmdResult);
}

void UMoverInputAbility::ProduceMoverInput_Implementation(UMoverComponent* MoverComp, int32 SimTimeMs, FMoverInputCmdContext& InputCmd) {}
