#include "MoverAbility/ChaosMoverLaunchAbility.h"

#include "Actor/YanHookProjectile.h"
#include "YanMoverAngelscriptLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "GameFramework/Pawn.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Messages/VerbMessageHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosMoverLaunchAbility)

UChaosMoverLaunchAbility::UChaosMoverLaunchAbility(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	// 服务器权威发射：投射物由服务器 Spawn 并复制；本地仅预测按下/松开表现，不预测对目标的击飞。
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UChaosMoverLaunchAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	PendingProjectile = nullptr;

	UAbilitySystemComponent* ASC       = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	APawn*                   OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());

	if (!ASC || !OwnerPawn || !ProjectileClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// 按下不发射，仅在本地受控端播放预测的蓄力/瞄准表现
	if (ActorInfo->IsLocallyControlled())
	{
		PlayLocalFeedback(PressSound, PressMontage);
	}

	// 等待松开：经通用复制事件转发，客户端与服务器两端的任务都会收到（bTestAlreadyReleased 兜底瞬按瞬放）
	WaitReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased=*/true);
	if (WaitReleaseTask)
	{
		WaitReleaseTask->OnRelease.AddDynamic(this, &UChaosMoverLaunchAbility::OnInputReleased);
		WaitReleaseTask->ReadyForActivation();
	}
}

void UChaosMoverLaunchAbility::OnInputReleased(float TimeHeld)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	// 本地受控端：松开瞬间播放预测的发射表现
	if (ActorInfo && ActorInfo->IsLocallyControlled())
	{
		PlayLocalFeedback(ReleaseSound, ReleaseMontage);
	}

	// 服务器权威：Spawn 投射物并 Launch；命中/未命中前技能保持激活，结束由服务器统一复制
	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		SpawnAndLaunchProjectile();
	}
}

void UChaosMoverLaunchAbility::SpawnAndLaunchProjectile()
{
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!OwnerPawn || !ProjectileClass)
	{
		K2_EndAbility();
		return;
	}

	UMoverComponent* MoverComp = OwnerPawn->FindComponentByClass<UMoverComponent>();

	// 方向取客户端上传服务器的输入 ControlRotation（含完整 Pitch），原点取 Pawn 视点，与命中口径一致
	const FCharacterDefaultInputs* CharInputs  = MoverComp ? MoverComp->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>() : nullptr;
	const FVector                  AimLocation = OwnerPawn->GetPawnViewLocation();
	const FRotator                 AimRotation = CharInputs ? CharInputs->ControlRotation : OwnerPawn->GetActorRotation();

	AYanHookProjectile* Projectile = UYanMoverAngelscriptLibrary::SpawnHookProjectile(MoverComp, ProjectileClass, AimLocation, AimRotation);
	if (!IsValid(Projectile))
	{
		K2_EndAbility();
		return;
	}

	PendingProjectile = Projectile;
	Projectile->OnHookHit.AddDynamic(this, &UChaosMoverLaunchAbility::HandleProjectileHit);
	Projectile->OnHookMissed.AddDynamic(this, &UChaosMoverLaunchAbility::HandleProjectileMissed);
	Projectile->Launch(AimRotation.Vector(), ProjectileSpeed);
}

FVector UChaosMoverLaunchAbility::ComputeLaunchVelocity(const AYanHookProjectile* Projectile) const
{
	// 击飞方向：投射物前向与 Up 按 UpwardRatio 插值后归一化
	const FVector LaunchDir = (Projectile->GetActorForwardVector() + FVector::UpVector * UpwardRatio).GetSafeNormal();
	return LaunchDir * LaunchSpeed;
}

void UChaosMoverLaunchAbility::HandleProjectileHit(AYanHookProjectile* Projectile, const FHitResult& Hit)
{
	UAbilitySystemComponent* InstigatorASC = GetAbilitySystemComponentFromActorInfo();
	APawn*                   HitPawn       = Cast<APawn>(Hit.GetActor());

	// 服务器权威击飞：以击飞 GE 施加到目标 ASC，由 UMoverLaunchExecutionCalculation 落到目标 Mover。
	// 命中表现挂在 GE 的 GameplayCues 上，随 GE 执行在各端播放。
	if (InstigatorASC && HitPawn && IsValid(Projectile) && LaunchGameplayEffect)
	{
		if (UAbilitySystemComponent* TargetASC =UVerbMessageHelpers::GetAbilitySystemComponentFromObject(HitPawn))
		{
			const FVector LaunchVelocity = ComputeLaunchVelocity(Projectile);
			// Chaos OverrideVelocity 为单帧瞬时速度，无持续时间语义，DurationMs 传 0
			UYanMoverAngelscriptLibrary::ApplyLaunchEffectToTarget(InstigatorASC, TargetASC, LaunchGameplayEffect, LaunchVelocity, /*DurationMs=*/0.f);
		}
	}

	// 命中非 Pawn、命中无 ASC 的目标或处理完毕：结束技能
	K2_EndAbility();
}

void UChaosMoverLaunchAbility::HandleProjectileMissed(AYanHookProjectile* Projectile)
{
	K2_EndAbility();
}

void UChaosMoverLaunchAbility::PlayLocalFeedback(USoundBase* Sound, UAnimMontage* Montage) const
{
	const APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	// 本地即时反馈（预测）：仅施法者本人听到/看到，不向其他端复制
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(OwnerPawn, Sound, OwnerPawn->GetActorLocation());
	}

	if (Montage)
	{
		if (const USkeletalMeshComponent* Mesh = OwnerPawn->FindComponentByClass<USkeletalMeshComponent>())
		{
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
			{
				AnimInstance->Montage_Play(Montage);
			}
		}
	}
}

void UChaosMoverLaunchAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (WaitReleaseTask)
	{
		WaitReleaseTask->EndTask();
		WaitReleaseTask = nullptr;
	}

	if (IsValid(PendingProjectile))
	{
		PendingProjectile->Destroy();
		PendingProjectile = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
