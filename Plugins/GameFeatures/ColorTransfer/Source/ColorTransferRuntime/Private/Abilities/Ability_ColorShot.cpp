#include "Abilities/Ability_ColorShot.h"

#include "AbilitySystemComponent.h"
#include "Actors/ColorShotBeam.h"
#include "Attributes/ColorPigmentSet.h"
#include "Effects/ColorPigmentEffects.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "MoverComponent.h"
#include "TimerManager.h"
#include "MoverLibrary/MoverQueryLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Ability_ColorShot)

UE_DEFINE_GAMEPLAY_TAG(Status_ColorShotBlocked, "Status.ColorShotBlocked");

UAbility_ColorShot::UAbility_ColorShot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = EModularAbilityActivationPolicy::OnInputTriggered;

	ActivationBlockedTags.AddTag(Status_ColorShotBlocked);
}

void UAbility_ColorShot::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!IsActive() || !HasAuthority(&ActivationInfo))
	{
		return;
	}

	APawn* Shooter = Cast<APawn>(ActorInfo->AvatarActor.Get());
	if (!Shooter)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Shooter;
	SpawnParams.Instigator = Shooter;
	SpawnParams.bDeferConstruction = true;

	Beam = GetWorld()->SpawnActor<AColorShotBeam>(BeamClass, Shooter->GetActorTransform(), SpawnParams);
	if (Beam)
	{
		// 射手的准星方向需经 Beam 上报，到达前先用输入朝向结算
		FVector ViewLocation;
		FVector ViewForward;
		UMoverComponent* MoverComp = Shooter->FindComponentByClass<UMoverComponent>();
		if (UMoverQueryLibrary::GetOwnerViewLocationAndForward(MoverComp, ViewLocation, ViewForward))
		{
			Beam->SetAimDirection(ViewForward);
		}
		Beam->SetMaxRange(MaxRange);
		Beam->SetImpactInterval(SprayInterval);
		Beam->FinishSpawning(Shooter->GetActorTransform());
	}

	SprayTick();
	GetWorld()->GetTimerManager().SetTimer(SprayTimer, this, &ThisClass::SprayTick, SprayInterval, /*bLoop=*/true);
}

void UAbility_ColorShot::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UAbility_ColorShot::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		GetWorld()->GetTimerManager().ClearTimer(SprayTimer);

		if (Beam)
		{
			Beam->Destroy();
			Beam = nullptr;
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAbility_ColorShot::SprayTick()
{
	// 发射途中进入禁射区域（如风场）即停止
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && ASC->HasMatchingGameplayTag(Status_ColorShotBlocked))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	AActor* Shooter = GetAvatarActorFromActorInfo();
	UMoverComponent* MoverComp = Shooter ? Shooter->FindComponentByClass<UMoverComponent>() : nullptr;
	UColorPigmentSet* ShooterPigments = UColorPigmentSet::FindForActor(Shooter);

	FVector ViewLocation;
	FVector ViewForward;
	if (!ShooterPigments || !MoverComp || !UMoverQueryLibrary::GetOwnerViewLocationAndForward(MoverComp, ViewLocation, ViewForward))
	{
		return;
	}

	// 与各端绘制的射线同源：方向取射手上报的准星方向
	const FVector AimDirection = Beam ? Beam->GetAimDirection() : ViewForward;

	EColorPigment Pigment = EColorPigment::None;
	const float Amount = ColorTransfer::AuthApplyShotConsumption(*ShooterPigments, PigmentPerSecond * SprayInterval, Pigment, Shooter);

	if (Beam)
	{
		Beam->AuthSetPigment(Pigment);
	}

	TArray<FVector> Points;
	FHitResult Hit;
	if (Amount <= 0.0f || !AColorShotBeam::PredictPath(GetWorld(), Shooter, ViewLocation, AimDirection, MaxRange, Points, Hit))
	{
		return;
	}

	AActor* HitActor = Hit.GetActor();
	if (IColorShotReceiver* Receiver = Cast<IColorShotReceiver>(HitActor))
	{
		FColorShotHit Shot;
		Shot.Pigment = Pigment;
		Shot.Amount = Amount;
		Shot.Hit = Hit;
		Receiver->ReceiveColorShot(Shot);
	}
	else if (UColorPigmentSet* TargetPigments = UColorPigmentSet::FindForActor(HitActor))
	{
		ColorTransfer::AuthApplyInjection(*TargetPigments, Pigment, Amount, Shooter);
	}
}
