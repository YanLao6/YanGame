#include "Abilities/Ability_ColorCharge.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Actors/ColorChargeOrb.h"
#include "Attributes/ColorPigmentSet.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Ability_ColorCharge)

namespace ColorCharge
{
	constexpr float OrbUpdateInterval = 0.1f;
}

UAbility_ColorCharge::UAbility_ColorCharge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = EModularAbilityActivationPolicy::OnInputTriggered;
}

bool UAbility_ColorCharge::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 外来颜料栈复制到各端，客户端预测与服务器裁决一致
	const UColorPigmentSet* PigmentSet = UColorPigmentSet::FindForActor(ActorInfo->AvatarActor.Get());
	return PigmentSet && PigmentSet->GetLayers().IsEmpty();
}

void UAbility_ColorCharge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!IsActive())
	{
		return;
	}

	PlayMontage(ChargeMontage, /*bStopWhenAbilityEnds=*/true);
	ChargeStartTime = GetWorld()->GetTimeSeconds();

	UAbilityTask_WaitInputRelease* WaitRelease = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased=*/true);
	WaitRelease->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
	WaitRelease->ReadyForActivation();

	APawn* Caster = Cast<APawn>(ActorInfo->AvatarActor.Get());
	if (!HasAuthority(&ActivationInfo) || !Caster || !OrbClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	// Owner 设为施法者，蓄力球才能沿施法者的连接发 Server RPC
	SpawnParams.Owner = Caster;
	SpawnParams.Instigator = Caster;
	Orb = GetWorld()->SpawnActor<AColorChargeOrb>(OrbClass, Caster->GetActorTransform(), SpawnParams);
	if (!Orb)
	{
		return;
	}

	if (USkeletalMeshComponent* Mesh = Caster->FindComponentByClass<USkeletalMeshComponent>())
	{
		Orb->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, OrbSocket);
	}

	UpdateOrbCharge();
	GetWorld()->GetTimerManager().SetTimer(ChargeTimer, this, &ThisClass::UpdateOrbCharge, ColorCharge::OrbUpdateInterval, /*bLoop=*/true);
}

void UAbility_ColorCharge::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		GetWorld()->GetTimerManager().ClearTimer(ChargeTimer);

		// 发射后蓄力球已交出，这里只清理未发射的
		if (Orb)
		{
			Orb->Destroy();
			Orb = nullptr;
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAbility_ColorCharge::HandleInputReleased(float TimeHeld)
{
	GetWorld()->GetTimerManager().ClearTimer(ChargeTimer);

	// 松手动画通常会跨过技能结束，需播完
	PlayMontage(ReleaseMontage, /*bStopWhenAbilityEnds=*/false);
	OnChargeReleased(FMath::Clamp(TimeHeld, 0.0f, MaxChargeTime));
}

void UAbility_ColorCharge::OnChargeReleased(float ChargeSeconds)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

float UAbility_ColorCharge::GetChargeAlpha(float ChargeSeconds) const
{
	return FMath::Clamp(ChargeSeconds / MaxChargeTime, 0.0f, 1.0f);
}

int32 UAbility_ColorCharge::GetChargeTier(float ChargeSeconds) const
{
	return FMath::Clamp(FMath::FloorToInt32(GetChargeAlpha(ChargeSeconds) * (ChargeTierCount - 1)), 0, ChargeTierCount - 1);
}

float UAbility_ColorCharge::GetHeldSeconds() const
{
	return FMath::Clamp(static_cast<float>(GetWorld()->GetTimeSeconds() - ChargeStartTime), 0.0f, MaxChargeTime);
}

void UAbility_ColorCharge::UpdateOrbCharge()
{
	if (Orb)
	{
		const float ChargeSeconds = GetHeldSeconds();
		Orb->AuthSetCharge(GetChargeAlpha(ChargeSeconds), GetChargeTier(ChargeSeconds));
	}
}

void UAbility_ColorCharge::PlayMontage(UAnimMontage* Montage, bool bStopWhenAbilityEnds)
{
	if (!Montage)
	{
		return;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, 1.0f, NAME_None, bStopWhenAbilityEnds);
	MontageTask->ReadyForActivation();
}
