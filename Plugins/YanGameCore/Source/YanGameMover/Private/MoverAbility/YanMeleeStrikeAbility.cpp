#include "MoverAbility/YanMeleeStrikeAbility.h"

#include "YanMoverAngelscriptLibrary.h"
#include "AbilitySystemComponent.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Messages/VerbMessageHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanMeleeStrikeAbility)

UYanMeleeStrikeAbility::UYanMeleeStrikeAbility(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	// 命中裁决与效果施加一律在服务器；本地端只预测挥击表现，不预测对目标的击退
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UYanMeleeStrikeAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!ActorInfo || !OwnerPawn)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	if (ActorInfo->IsLocallyControlled())
	{
		PlayLocalFeedback();
		OnStrikePerformed();
	}

	if (ActorInfo->IsNetAuthority())
	{
		// 瞄准口径与项目其他 Mover 技能一致：原点取视点，方向取输入包上传的 ControlRotation，
		// 服务器据此能用与客户端相同的数据复核命中
		const UMoverComponent*         MoverComp   = OwnerPawn->FindComponentByClass<UMoverComponent>();
		const FCharacterDefaultInputs* CharInputs  = MoverComp ? MoverComp->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>() : nullptr;
		const FVector                  Origin      = OwnerPawn->GetPawnViewLocation();
		const FRotator                 AimRotation = CharInputs ? CharInputs->ControlRotation : OwnerPawn->GetActorRotation();
		const FVector                  AimForward  = AimRotation.Vector();

		TArray<APawn*> Targets;
		GatherTargets(Origin, AimForward, Targets);

		// 击退方向对全部目标取同一个：玩家瞄哪打飞哪，落点可预测，也便于服务器复核
		const FVector KnockbackVelocity = (AimForward + FVector::UpVector * UpwardRatio).GetSafeNormal() * KnockbackSpeed;

		UAbilitySystemComponent* InstigatorASC = GetAbilitySystemComponentFromActorInfo();
		for (APawn* Target : Targets)
		{
			ApplyStrikeToTarget(InstigatorASC, Target, KnockbackVelocity);
		}
	}
}

void UYanMeleeStrikeAbility::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UYanMeleeStrikeAbility::GatherTargets(const FVector& Origin, const FVector& AimForward, TArray<APawn*>& OutTargets) const
{
	const AActor* OwnerActor = GetAvatarActorFromActorInfo();
	const UWorld* World      = OwnerActor ? OwnerActor->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MeleeStrikeGather), /*bTraceComplex=*/false);
	QueryParams.AddIgnoredActor(OwnerActor);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps,
	                                Origin,
	                                FQuat::Identity,
	                                FCollisionObjectQueryParams(ECC_Pawn),
	                                FCollisionShape::MakeSphere(AttackRange),
	                                QueryParams);

	const float MinAimDot = FMath::Cos(FMath::DegreesToRadians(AttackHalfAngle));

	for (const FOverlapResult& Overlap : Overlaps)
	{
		APawn* Target = Cast<APawn>(Overlap.GetActor());

		// 同一 Actor 可能有多个组件落在球内，去重后才逐个判定
		if (!Target || Target == OwnerActor || OutTargets.Contains(Target))
		{
			continue;
		}

		if (!UVerbMessageHelpers::GetAbilitySystemComponentFromObject(Target))
		{
			continue;
		}

		const FVector ToTarget    = Target->GetActorLocation() - Origin;
		const FVector ToTargetDir = ToTarget.GetSafeNormal();
		if (ToTargetDir.IsNearlyZero() || FVector::DotProduct(AimForward, ToTargetDir) < MinAimDot)
		{
			continue;
		}

		// 隔墙不该打得到：扇形只约束方向与距离，遮挡须另行判定
		FHitResult            OcclusionHit;
		FCollisionQueryParams OcclusionParams(SCENE_QUERY_STAT(MeleeStrikeOcclusion), /*bTraceComplex=*/false);
		OcclusionParams.AddIgnoredActor(OwnerActor);
		OcclusionParams.AddIgnoredActor(Target);

		if (World->LineTraceSingleByChannel(OcclusionHit, Origin, Target->GetActorLocation(), ECC_Visibility, OcclusionParams))
		{
			continue;
		}

		OutTargets.Add(Target);
	}
}

void UYanMeleeStrikeAbility::ApplyStrikeToTarget(UAbilitySystemComponent* InstigatorASC, APawn* Target, const FVector& KnockbackVelocity) const
{
	UAbilitySystemComponent* TargetASC = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(Target);
	if (!InstigatorASC || !TargetASC)
	{
		return;
	}

	// 先压住落地：击退若在地面模式下结算，水平速度当帧就会被地面摩擦抵消
	if (NoLandingDuration > 0.f)
	{
		if (UMoverComponent* TargetMover = Target->FindComponentByClass<UMoverComponent>())
		{
			UYanMoverAngelscriptLibrary::ApplyNoLandingToTarget(TargetMover, NoLandingDuration);
		}
	}

	if (KnockbackEffect && !KnockbackVelocity.IsNearlyZero())
	{
		// Chaos OverrideVelocity 为单帧瞬时速度，无持续时间语义，DurationMs 传 0
		UYanMoverAngelscriptLibrary::ApplyLaunchEffectToTarget(InstigatorASC, TargetASC, KnockbackEffect, KnockbackVelocity, /*DurationMs=*/0.f);
	}

	for (const TSubclassOf<UGameplayEffect>& EffectClass : HitEffects)
	{
		if (!EffectClass)
		{
			continue;
		}

		FGameplayEffectContextHandle Context = InstigatorASC->MakeEffectContext();
		Context.AddInstigator(InstigatorASC->GetAvatarActor(), InstigatorASC->GetAvatarActor());

		const FGameplayEffectSpecHandle Spec = InstigatorASC->MakeOutgoingSpec(EffectClass, GetAbilityLevel(), Context);
		if (Spec.IsValid())
		{
			InstigatorASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
		}
	}
}

void UYanMeleeStrikeAbility::PlayLocalFeedback() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	// 本地即时反馈（预测）：仅施法者本人听到/看到，不向其他端复制。
	// 需要各端都表现的命中效果挂在 GE 的 GameplayCue 上
	if (StrikeSound)
	{
		UGameplayStatics::PlaySoundAtLocation(OwnerPawn, StrikeSound, OwnerPawn->GetActorLocation());
	}

	if (StrikeMontage)
	{
		if (const USkeletalMeshComponent* Mesh = OwnerPawn->FindComponentByClass<USkeletalMeshComponent>())
		{
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
			{
				AnimInstance->Montage_Play(StrikeMontage);
			}
		}
	}
}
