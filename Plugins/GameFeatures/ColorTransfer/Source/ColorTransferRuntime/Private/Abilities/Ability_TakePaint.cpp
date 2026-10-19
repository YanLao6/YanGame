#include "Abilities/Ability_TakePaint.h"

#include "ColorTransferTypes.h"
#include "Attributes/ColorPigmentSet.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Ability_TakePaint)

UAbility_TakePaint::UAbility_TakePaint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UAbility_TakePaint::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!IsActive())
	{
		return;
	}

	if (HasAuthority(&ActivationInfo) && TriggerEventData)
	{
		const AActor* Avatar = ActorInfo->AvatarActor.Get();
		AActor* SourceActor = const_cast<AActor*>(TriggerEventData->Target.Get());
		IColorPigmentSource* PigmentSource = Cast<IColorPigmentSource>(SourceActor);
		if (Avatar && PigmentSource && FVector::DistSquared(Avatar->GetActorLocation(), SourceActor->GetActorLocation()) <= FMath::Square(MaxInteractDistance))
		{
			PigmentSource->AuthTakeTopPigment(UColorPigmentSet::FindForActor(Avatar), AmountPerInteract);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}
