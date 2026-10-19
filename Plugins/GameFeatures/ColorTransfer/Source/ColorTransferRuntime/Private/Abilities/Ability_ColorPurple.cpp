#include "Abilities/Ability_ColorPurple.h"

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemComponent.h"
#include "Actors/ColorPurpleProjectile.h"
#include "ColorTransferQueries.h"
#include "ColorTransferTypes.h"
#include "Components/ColorPigmentRulesComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Ability_ColorPurple)

UAbility_ColorPurple::UAbility_ColorPurple(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy   = EModularAbilityActivationPolicy::OnInputTriggered;

	ActivationRequiredTags.AddTag(Status_PurpleCharging);
}

void UAbility_ColorPurple::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!IsActive())
	{
		return;
	}

	if (HasAuthority(&ActivationInfo))
	{
		if (!AuthBeginCharge())
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
			return;
		}

		if (!ActorInfo->IsLocallyControlled())
		{
			UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
			TargetDataHandle             = ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).AddUObject(this, &ThisClass::OnTargetDataReady);
		}
	}

	PlayMontage(ChargeMontage);

	UAbilityTask_WaitInputRelease* WaitRelease = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased=*/true);
	WaitRelease->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
	WaitRelease->ReadyForActivation();
}

void UAbility_ColorPurple::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		if (TargetDataHandle.IsValid())
		{
			UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
			ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).Remove(TargetDataHandle);
			ASC->ConsumeClientReplicatedTargetData(Handle, ActivationInfo.GetActivationPredictionKey());
			TargetDataHandle.Reset();
		}

		// 发射后茈已交出，这里只清理未发射的
		if (Projectile)
		{
			Projectile->Destroy();
			Projectile = nullptr;
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UAbility_ColorPurple::AuthBeginCharge()
{
	// 规则组件与 ASC 同在 PlayerState 上
	const AActor*                OwnerActor = GetOwningActorFromActorInfo();
	UColorPigmentRulesComponent* Rules      = OwnerActor ? OwnerActor->FindComponentByClass<UColorPigmentRulesComponent>() : nullptr;
	if (!Rules || !Rules->AuthCancelPurple())
	{
		return false;
	}

	APawn* Shooter = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!Shooter || !ProjectileClass)
	{
		return true;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner                          = Shooter;
	SpawnParams.Instigator                     = Shooter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Projectile                                 = GetWorld()->SpawnActor<AColorPurpleProjectile>(ProjectileClass, Shooter->GetActorTransform(), SpawnParams);
	if (!Projectile)
	{
		return true;
	}

	if (USkeletalMeshComponent* Mesh = Shooter->FindComponentByClass<USkeletalMeshComponent>())
	{
		Projectile->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, ProjectileSocket);
	}

	return true;
}

void UAbility_ColorPurple::HandleInputReleased(float TimeHeld)
{
	PlayMontage(FireMontage);

	// 瞄准只能由本地控制端做，服务器上的远端玩家在 OnTargetDataReady 里等结果
	if (!CurrentActorInfo->IsLocallyControlled())
	{
		return;
	}

	APawn*  Shooter = Cast<APawn>(GetAvatarActorFromActorInfo());
	FVector Start;
	FVector Direction;
	if (!ColorTransfer::GetLocalCameraAim(Shooter, /*bStartAtPawn=*/true, Start, Direction))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	const FVector         End = Start + Direction * AimRange;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ColorPurple), false, Shooter);
	FHitResult            AimHit;
	if (!GetWorld()->LineTraceSingleByChannel(AimHit, Start, End, ColorTransfer::ShotTraceChannel, Params))
	{
		AimHit             = FHitResult(Start, End);
		AimHit.Location    = End;
		AimHit.ImpactPoint = End;
	}

	if (HasAuthority(&CurrentActivationInfo))
	{
		AuthFire(AimHit);
		return;
	}

	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	FScopedPredictionWindow  ScopedPrediction(ASC);

	FGameplayAbilityTargetDataHandle Data(new FGameplayAbilityTargetData_SingleTargetHit(AimHit));
	ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(), Data, FGameplayTag(), ASC->ScopedPredictionKey);
}

void UAbility_ColorPurple::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());

	const FHitResult* AimHit = Data.Num() > 0 ? Data.Get(0)->GetHitResult() : nullptr;
	if (!AimHit)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	AuthFire(*AimHit);
}

void UAbility_ColorPurple::AuthFire(const FHitResult& AimHit)
{
	AColorPurpleProjectile* FiredProjectile = Projectile;
	Projectile                              = nullptr;

	if (FiredProjectile)
	{
		// 从插槽飞向瞄准点，而不是沿相机射线平移，第三人称下才能真正命中准星所指
		const FVector AimPoint  = AimHit.bBlockingHit ? FVector(AimHit.ImpactPoint) : FVector(AimHit.TraceEnd);
		FVector       Direction = (AimPoint - FiredProjectile->GetActorLocation()).GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			Direction = (AimHit.TraceEnd - AimHit.TraceStart).GetSafeNormal();
		}
		FiredProjectile->AuthLaunch(Direction);
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UAbility_ColorPurple::PlayMontage(UAnimMontage* Montage)
{
	if (!Montage)
	{
		return;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, 1.0f, NAME_None, /*bStopWhenAbilityEnds=*/true);
	MontageTask->ReadyForActivation();
}
