#include "MoverAbility/MoverInputAbility.h"

#include "MoverComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverInputAbility)

void UMoverInputAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CachedMoverComp)
	{
		RefreshMoverRegistration(ActorInfo);
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UMoverInputAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	ensureMsgf(GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor,
	           TEXT("%s 必须为 InstancedPerActor，否则输入源登记无法注销"), *GetName());

	ensureMsgf(GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted ||
	           GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalOnly,
	           TEXT("%s 的 NetExecutionPolicy 须为 LocalPredicted 或 LocalOnly，否则意图写不进输入包"), *GetName());

	Super::OnGiveAbility(ActorInfo, Spec);
}

void UMoverInputAbility::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnRemoveAbility(ActorInfo, Spec);

	UnregisterFromMover();
}

void UMoverInputAbility::OnPawnAvatarSet()
{
	Super::OnPawnAvatarSet();

	// 钩子只向实例分发，此处兜底 CDO 误调：CDO 无 CurrentActorInfo 亦不得持有登记
	if (!IsInstantiated())
	{
		return;
	}

	// Avatar 至此才确定为 Pawn；CurrentActorInfo 已在 OnGiveAbility 中建立
	RefreshMoverRegistration(GetCurrentActorInfo());
}

void UMoverInputAbility::RefreshMoverRegistration(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!IsInstantiated())
	{
		return;
	}

	const AActor*    Avatar       = (ActorInfo != nullptr) ? ActorInfo->AvatarActor.Get() : nullptr;
	UMoverComponent* NewMoverComp = (Avatar != nullptr) ? Avatar->FindComponentByClass<UMoverComponent>() : nullptr;

	SetMoverComponent(NewMoverComp);
}

void UMoverInputAbility::UnregisterFromMover()
{
	SetMoverComponent(nullptr);
}

void UMoverInputAbility::SetMoverComponent(UMoverComponent* NewMoverComp)
{
	UMoverComponent* const OldMoverComp = CachedMoverComp;
	if (OldMoverComp == NewMoverComp)
	{
		return;
	}

	if (IsValid(OldMoverComp))
	{
		OldMoverComp->InputProducers.Remove(this);
	}

	CachedMoverComp = NewMoverComp;
	if (CachedMoverComp)
	{
		CachedMoverComp->InputProducers.AddUnique(this);
	}

	OnMoverComponentSet(OldMoverComp, CachedMoverComp);
}

void UMoverInputAbility::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	ProduceMoverInput(CachedMoverComp, SimTimeMs, InputCmdResult);
}

void UMoverInputAbility::ProduceMoverInput_Implementation(UMoverComponent* MoverComp, int32 SimTimeMs, FMoverInputCmdContext& InputCmd) {}
