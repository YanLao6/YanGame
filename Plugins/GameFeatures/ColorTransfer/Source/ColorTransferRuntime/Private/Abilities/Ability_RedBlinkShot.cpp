#include "Abilities/Ability_RedBlinkShot.h"

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystemComponent.h"
#include "Actors/ColorAttractOrb.h"
#include "Actors/ColorChargeOrb.h"
#include "Actors/ColorFusionOrb.h"
#include "Attributes/ColorPigmentSet.h"
#include "Effects/ColorPigmentEffects.h"
#include "ColorTransferQueries.h"
#include "ColorTransferTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "MoverComponent.h"
#include "MoverLibrary/MoverAuthorityLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Ability_RedBlinkShot)

UAbility_RedBlinkShot::UAbility_RedBlinkShot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OrbSocket = TEXT("finger_r");
}

void UAbility_RedBlinkShot::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (IsActive() && HasAuthority(&ActivationInfo) && !ActorInfo->IsLocallyControlled())
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		TargetDataHandle = ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).AddUObject(this, &ThisClass::OnTargetDataReady);
	}
}

void UAbility_RedBlinkShot::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsEndAbilityValid(Handle, ActorInfo) && TargetDataHandle.IsValid())
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).Remove(TargetDataHandle);
		ASC->ConsumeClientReplicatedTargetData(Handle, ActivationInfo.GetActivationPredictionKey());
		TargetDataHandle.Reset();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

float UAbility_RedBlinkShot::GetRange(float ChargeSeconds) const
{
	return FMath::Lerp(RangeByCharge.X, RangeByCharge.Y, static_cast<float>(GetChargeTier(ChargeSeconds)) / FMath::Max(ChargeTierCount - 1, 1));
}

float UAbility_RedBlinkShot::GetFiredPigment() const
{
	return PigmentByTier.IsEmpty() ? 0.0f : PigmentByTier[FMath::Min(FiredTier, PigmentByTier.Num() - 1)];
}

float UAbility_RedBlinkShot::GetFiredLaunchSpeed() const
{
	return FMath::Lerp(LaunchSpeedByCharge.X, LaunchSpeedByCharge.Y, static_cast<float>(FiredTier) / FMath::Max(ChargeTierCount - 1, 1));
}

void UAbility_RedBlinkShot::TraceAttractOrbs(FHitResult& InOutHit) const
{
	const FVector Start = InOutHit.TraceStart;
	const FVector Segment = InOutHit.TraceEnd - Start;
	const double SegmentLength = Segment.Size();
	if (SegmentLength <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector Direction = Segment / SegmentLength;
	double NearestDistance = InOutHit.bBlockingHit ? FVector::Dist(Start, InOutHit.ImpactPoint) : SegmentLength;

	// 吸引球不参与碰撞（否则会截断其他颜色射线），按网格包围球求交
	for (TActorIterator<AColorAttractOrb> It(GetWorld()); It; ++It)
	{
		AColorAttractOrb* AttractOrb = *It;
		UPrimitiveComponent* OrbComponent = Cast<UPrimitiveComponent>(AttractOrb->GetRootComponent());
		if (!OrbComponent)
		{
			continue;
		}

		const FVector Center = OrbComponent->Bounds.Origin;
		const double RadiusSquared = FMath::Square(OrbComponent->Bounds.SphereRadius);
		const FVector ToCenter = Center - Start;
		const double Along = ToCenter | Direction;
		const double MissSquared = ToCenter.SizeSquared() - FMath::Square(Along);
		if (MissSquared > RadiusSquared)
		{
			continue;
		}

		const double HalfChord = FMath::Sqrt(RadiusSquared - MissSquared);
		if (Along + HalfChord < 0.0)
		{
			continue;
		}

		const double HitDistance = FMath::Max(Along - HalfChord, 0.0);
		if (HitDistance >= NearestDistance)
		{
			continue;
		}

		const FVector HitLocation = Start + Direction * HitDistance;
		FHitResult OrbHit(AttractOrb, OrbComponent, HitLocation, (HitLocation - Center).GetSafeNormal());
		OrbHit.TraceStart = Start;
		OrbHit.TraceEnd = InOutHit.TraceEnd;
		OrbHit.bBlockingHit = true;
		OrbHit.Distance = HitDistance;
		OrbHit.Time = HitDistance / SegmentLength;
		InOutHit = OrbHit;
		NearestDistance = HitDistance;
	}
}

void UAbility_RedBlinkShot::OnChargeReleased(float ChargeSeconds)
{
	// 瞄准只能由本地控制端做，服务器上的远端玩家在 OnTargetDataReady 里等结果
	if (!CurrentActorInfo->IsLocallyControlled())
	{
		return;
	}

	APawn* Shooter = Cast<APawn>(GetAvatarActorFromActorInfo());
	FVector Start;
	FVector Direction;
	if (!ColorTransfer::GetLocalCameraAim(Shooter, /*bStartAtPawn=*/true, Start, Direction))
	{
		Super::OnChargeReleased(ChargeSeconds);
		return;
	}

	const FVector End = Start + Direction * GetRange(ChargeSeconds);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RedBlinkShot), false, Shooter);
	FHitResult AimHit;
	if (!GetWorld()->LineTraceSingleByChannel(AimHit, Start, End, ColorTransfer::ShotTraceChannel, Params))
	{
		AimHit = FHitResult(Start, End);
		AimHit.Location = End;
		AimHit.ImpactPoint = End;
	}
	TraceAttractOrbs(AimHit);

	if (HasAuthority(&CurrentActivationInfo))
	{
		AuthFire(AimHit);
		return;
	}

	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	FScopedPredictionWindow ScopedPrediction(ASC);

	FGameplayAbilityTargetDataHandle Data(new FGameplayAbilityTargetData_SingleTargetHit(AimHit));
	ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(), Data, FGameplayTag(), ASC->ScopedPredictionKey);
}

void UAbility_RedBlinkShot::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
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

void UAbility_RedBlinkShot::AuthFire(const FHitResult& AimHit)
{
	const FVector Start = AimHit.TraceStart;
	const FVector Direction = (AimHit.TraceEnd - Start).GetSafeNormal();

	// 射程以服务器计时为准，客户端上报的命中点超出射程时按未命中处理
	const float HeldSeconds = GetHeldSeconds();
	FiredTier = GetChargeTier(HeldSeconds);
	const float Range = GetRange(HeldSeconds);
	const float HitDistance = FVector::Dist(Start, AimHit.ImpactPoint);
	const bool bInRange = AimHit.bBlockingHit && HitDistance <= Range;

	AColorChargeOrb* FlyingOrb = Orb;
	Orb = nullptr;
	if (!FlyingOrb)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
		return;
	}

	AColorAttractOrb* HitAttractOrb = bInRange ? Cast<AColorAttractOrb>(AimHit.GetActor()) : nullptr;
	if (IsValid(HitAttractOrb))
	{
		// 吸引球会移动，红球追踪其球心；技能保持激活直到结算完
		Orb = FlyingOrb;
		FlyingOrb->OnArrived.BindWeakLambda(this, [this, WeakAttractOrb = TWeakObjectPtr<AColorAttractOrb>(HitAttractOrb)]()
		{
			AuthHandleAttractOrbHit(WeakAttractOrb);
		});
		FlyingOrb->AuthFlyTo(AimHit.ImpactPoint, HitAttractOrb->GetRootComponent());
		return;
	}

	APawn* Target = bInRange ? Cast<APawn>(AimHit.GetActor()) : nullptr;
	USkeletalMeshComponent* TargetMesh = Target ? Target->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	if (Target && TargetMesh)
	{
		// 红球只是表现，飞到对方身上才生效；技能保持激活直到结算完，被取消时由 EndAbility 回收红球
		Orb = FlyingOrb;
		FlyingOrb->OnArrived.BindWeakLambda(this, [this, WeakTarget = TWeakObjectPtr<APawn>(Target), Direction]()
		{
			AuthHandleOrbArrived(WeakTarget, Direction);
		});
		FlyingOrb->AuthFlyTo(AimHit.ImpactPoint, TargetMesh, HitAttachSocket);
		return;
	}

	const FVector Destination = bInRange ? FVector(AimHit.ImpactPoint) : Start + Direction * Range;
	if (bInRange)
	{
		FlyingOrb->OnArrived.BindWeakLambda(FlyingOrb, [FlyingOrb, WeakComponent = TWeakObjectPtr<UPrimitiveComponent>(AimHit.GetComponent())]()
		{
			AColorFusionOrb::AuthNotifyChargeHit(WeakComponent.Get());
			FlyingOrb->Destroy();
		});
	}
	FlyingOrb->AuthFlyTo(Destination);

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UAbility_RedBlinkShot::AuthHandleOrbArrived(TWeakObjectPtr<APawn> WeakTarget, FVector Direction)
{
	AColorChargeOrb* ArrivedOrb = Orb;
	Orb = nullptr;

	APawn* Target = WeakTarget.Get();
	USkeletalMeshComponent* TargetMesh = Target ? Target->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	if (!Target || !TargetMesh || !ArrivedOrb)
	{
		if (ArrivedOrb)
		{
			ArrivedOrb->Destroy();
		}
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
		return;
	}

	if (UColorPigmentSet* TargetPigments = UColorPigmentSet::FindForActor(Target))
	{
		ColorTransfer::AuthApplyInjection(*TargetPigments, EColorPigment::Red, GetFiredPigment(), GetAvatarActorFromActorInfo());
	}

	if (UMoverComponent* TargetMover = Target->FindComponentByClass<UMoverComponent>())
	{
		// 覆盖而非叠加，击飞强度不受对方当前跑动方向影响
		const FVector LaunchVelocity = Direction * GetFiredLaunchSpeed() + FVector::UpVector * LaunchUpSpeed;
		UMoverAuthorityLibrary::ApplyChaosVelocity(TargetMover, LaunchVelocity, /*bOverrideVelocity=*/true, /*bScheduleForSync=*/true);
		if (NoLandingDuration > 0.0f)
		{
			UMoverAuthorityLibrary::ApplyNoLanding(TargetMover, NoLandingDuration);
		}
	}

	// 红球随对方一起飞出，停留片刻后消失
	ArrivedOrb->AttachToComponent(TargetMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HitAttachSocket);
	ArrivedOrb->SetLifeSpan(OrbLingerTime + UE_KINDA_SMALL_NUMBER);

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UAbility_RedBlinkShot::AuthHandleAttractOrbHit(TWeakObjectPtr<AColorAttractOrb> WeakAttractOrb)
{
	AColorChargeOrb* ArrivedOrb = Orb;
	Orb = nullptr;

	if (ArrivedOrb)
	{
		// 吸引球已在飞行途中消失时红球直接消散，不爆炸
		if (AColorAttractOrb* AttractOrb = WeakAttractOrb.Get())
		{
			const FVector Location = ArrivedOrb->GetActorLocation();
			const float Amount = GetFiredPigment();

			// 紫色即红蓝等量：两种都注入，被注入者自身的颜色跳过，否则会冲掉外来颜料
			for (APawn* Pawn : ColorTransfer::GatherPawnsInShape(GetWorld(), Location, FQuat::Identity, FCollisionShape::MakeSphere(DetonateRadius)))
			{
				UColorPigmentSet* Pigments = UColorPigmentSet::FindForActor(Pawn);
				if (!Pigments)
				{
					continue;
				}

				for (const EColorPigment Pigment : {EColorPigment::Red, EColorPigment::Blue})
				{
					if (Pigment != Pigments->GetOwnPigment())
					{
						ColorTransfer::AuthApplyInjection(*Pigments, Pigment, Amount, GetAvatarActorFromActorInfo());
					}
				}
			}

			AttractOrb->AuthDetonate(Location);
		}

		ArrivedOrb->Destroy();
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}
