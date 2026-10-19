#include "MoverLibrary/MoverTargetingLibrary.h"

#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "YanMoverDataModelTypes.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"
#include "Components/CapsuleComponent.h"
#include "Messages/VerbMessageHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverTargetingLibrary)

bool UMoverTargetingLibrary::LineTraceForGrapple(UMoverComponent* MoverComp, float MaxDistance, FVector& OutImpactPoint)
{
	if (!MoverComp)
	{
		return false;
	}
	const APawn* Pawn = Cast<APawn>(MoverComp->GetOwner());
	if (!Pawn)
	{
		return false;
	}
	// 取位口径与 GetOwnerViewLocationAndForward 一致：原点用 Pawn 视点、方向用输入 ControlRotation，
	// 与其他瞄准逻辑共享取位规则；历史命中校验仍需对应模拟帧的快照。
	const FVector                  Start       = Pawn->GetPawnViewLocation();
	const FCharacterDefaultInputs* CharInputs  = MoverComp->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FRotator                 AimRotation = CharInputs ? CharInputs->ControlRotation : Pawn->GetActorRotation();
	const FVector                  Forward     = AimRotation.Vector();
	const FVector                  End         = Start + Forward * MaxDistance;

	FHitResult            HitResult;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GrappleTrace), false);
	Params.AddIgnoredActor(Pawn);

	const bool bHit = MoverComp->GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params);
	if (bHit)
	{
		OutImpactPoint = HitResult.ImpactPoint;
	}
	return bHit;
}

bool UMoverTargetingLibrary::SweepBlinkTarget(UMoverComponent* MoverComp, FVector Direction, float MaxDistance, FVector& OutTargetLocation)
{
	if (!MoverComp || MaxDistance <= 0.f)
	{
		return false;
	}

	const UPrimitiveComponent* UpdatedPrimitive = Cast<UPrimitiveComponent>(MoverComp->GetUpdatedComponent());
	if (!UpdatedPrimitive)
	{
		return false;
	}

	const FVector BlinkDir = Direction.GetSafeNormal();
	if (BlinkDir.IsNearlyZero())
	{
		return false;
	}

	// 用角色自身的胶囊做扫描，落点才保证容得下角色；取不到胶囊时退化为射线，
	// 此时落点仅保证视线可达，可能贴得过近
	const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(UpdatedPrimitive);
	const FCollisionShape    Shape   = Capsule
		                                   ? FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight())
		                                   : FCollisionShape::MakeSphere(1.f);

	const FVector Start = UpdatedPrimitive->GetComponentLocation();
	const FVector End   = Start + BlinkDir * MaxDistance;

	FHitResult            HitResult;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BlinkTargetSweep), false);
	Params.AddIgnoredActor(MoverComp->GetOwner());

	const bool bBlocked = MoverComp->GetWorld()->SweepSingleByChannel(
		HitResult,
		Start,
		End,
		UpdatedPrimitive->GetComponentQuat(),
		UpdatedPrimitive->GetCollisionObjectType(),
		Shape,
		Params);

	// 命中时 HitResult.Location 已是形状停靠位置，再沿反向退开一点，
	// 避免终点紧贴表面导致后续移动被判定为持续碰撞
	constexpr float SurfaceClearance = 2.f;
	OutTargetLocation                = bBlocked ? (HitResult.Location - BlinkDir * SurfaceClearance) : End;

	return true;
}

APawn* UMoverTargetingLibrary::FindClinchTarget(UMoverComponent* MoverComp,
                                                     float            SearchRadius,
                                                     float            HalfAngleDegrees,
                                                     float            SurfaceClearance,
                                                     FVector&         OutAnchorPoint)
{
	AActor* OwnerActor = MoverComp ? MoverComp->GetOwner() : nullptr;
	UWorld* World      = OwnerActor ? OwnerActor->GetWorld() : nullptr;
	if (!World || SearchRadius <= 0.f)
	{
		return nullptr;
	}

	// 撞上去才算命中，故以速度为方向基准；速度过小时无从判断撞击方向，退化到角色朝向
	FVector ImpactDir = MoverComp->GetVelocity().GetSafeNormal2D();
	if (ImpactDir.IsNearlyZero())
	{
		ImpactDir = OwnerActor->GetActorForwardVector().GetSafeNormal2D();
	}

	const FVector Origin = OwnerActor->GetActorLocation();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ClinchTargetGather), /*bTraceComplex=*/false);
	QueryParams.AddIgnoredActor(OwnerActor);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps,
	                                Origin,
	                                FQuat::Identity,
	                                FCollisionObjectQueryParams(ECC_Pawn),
	                                FCollisionShape::MakeSphere(SearchRadius),
	                                QueryParams);

	const float MinImpactDot = FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));

	APawn* BestTarget      = nullptr;
	float  BestDistSquared = TNumericLimits<float>::Max();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		APawn* Candidate = Cast<APawn>(Overlap.GetActor());

		// 同一 Actor 可能有多个组件落在球内，命中最近者即可，无须去重
		if (!Candidate || Candidate == OwnerActor)
		{
			continue;
		}

		if (!UVerbMessageHelpers::GetAbilitySystemComponentFromObject(Candidate))
		{
			continue;
		}

		const FVector ToTarget    = Candidate->GetActorLocation() - Origin;
		const FVector ToTargetDir = ToTarget.GetSafeNormal2D();
		if (ToTargetDir.IsNearlyZero() || FVector::DotProduct(ImpactDir, ToTargetDir) < MinImpactDot)
		{
			continue;
		}

		const float DistSquared = ToTarget.SizeSquared();
		if (DistSquared < BestDistSquared)
		{
			BestDistSquared = DistSquared;
			BestTarget      = Candidate;
		}
	}

	if (!BestTarget)
	{
		return nullptr;
	}

	// 贴身距离取双方胶囊半径之和：小于此值两个物理体即产生穿透，
	// 接触约束会持续把贴住的双方推开
	float ClinchDistance = SurfaceClearance;
	if (const UCapsuleComponent* OwnCapsule = Cast<UCapsuleComponent>(MoverComp->GetUpdatedComponent()))
	{
		ClinchDistance += OwnCapsule->GetScaledCapsuleRadius();
	}
	if (const UCapsuleComponent* TargetCapsule = BestTarget->FindComponentByClass<UCapsuleComponent>())
	{
		ClinchDistance += TargetCapsule->GetScaledCapsuleRadius();
	}

	const FVector TargetLocation = BestTarget->GetActorLocation();

	// 从目标指向发起方的水平方向：抓取者停在自己冲来的那一侧，而非穿到对面
	FVector AwayDir = (Origin - TargetLocation).GetSafeNormal2D();
	if (AwayDir.IsNearlyZero())
	{
		AwayDir = -ImpactDir;
	}

	// 与目标等高：双方靠同一运动方程保持同步下落，存在高度差则一方先落地，守恒即被打破
	OutAnchorPoint = TargetLocation + AwayDir * ClinchDistance;

	return BestTarget;
}

bool UMoverTargetingLibrary::TraceWallGrindPath(UMoverComponent* MoverComp,
                                                     FVector          TargetLocation,
                                                     float            WallProbeDistance,
                                                     float            PairSeparation,
                                                     float            StepDistance,
                                                     float            Deceleration,
                                                     int32            MaxPoints,
                                                     TArray<FVector>& OutPath,
                                                     float&           OutEntrySpeed)
{
	AActor* OwnerActor = MoverComp ? MoverComp->GetOwner() : nullptr;
	UWorld* World      = OwnerActor ? OwnerActor->GetWorld() : nullptr;
	if (!World || WallProbeDistance <= 0.f || StepDistance <= 0.f)
	{
		return false;
	}

	// 推进方向以速度为基准：本机制由撞击触发，判定的是「往哪撞」而非「往哪看」
	const FVector PushDir = MoverComp->GetVelocity().GetSafeNormal2D();
	if (PushDir.IsNearlyZero())
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WallGrindPathTrace), /*bTraceComplex=*/false);
	QueryParams.AddIgnoredActor(OwnerActor);

	// 自被推方位置沿推进方向探墙：墙在被推方的另一侧，而非发起方的另一侧
	FHitResult WallHit;
	if (!World->LineTraceSingleByChannel(WallHit, TargetLocation, TargetLocation + PushDir * WallProbeDistance, ECC_WorldStatic, QueryParams))
	{
		return false;
	}

	// 只取水平分量：本机制限定竖直墙面，斜面与天花板不在覆盖范围内
	FVector WallNormal = WallHit.ImpactNormal.GetSafeNormal2D();
	if (WallNormal.IsNearlyZero())
	{
		return false;
	}

	// 沿墙方向为推进方向在墙面内的投影；正面垂直撞墙时无侧向分量，不构成推进
	FVector SlideDir = (PushDir - PushDir.Dot(WallNormal) * WallNormal).GetSafeNormal2D();
	if (SlideDir.IsNearlyZero())
	{
		return false;
	}

	OutEntrySpeed = static_cast<float>(FVector::DotProduct(MoverComp->GetVelocity(), SlideDir));
	if (OutEntrySpeed <= 0.f)
	{
		return false;
	}

	// 折线只需覆盖到理论停止距离；不减速时退化为按点数上限铺满
	const double StopDistance = (Deceleration > 0.f)
	                                ? FMath::Square(static_cast<double>(OutEntrySpeed)) / (2.0 * Deceleration)
	                                : static_cast<double>(StepDistance) * MaxPoints;

	const int32 PointBudget = FMath::Clamp(MaxPoints, 2, YanMoverInputs::MaxWallGrindPathPoints);

	OutPath.Reset();

	// 折线取发起方的行进位置：贴墙点再沿法线外推一个身位
	FVector CurrentPoint = WallHit.ImpactPoint + WallNormal * PairSeparation;
	CurrentPoint.Z       = TargetLocation.Z;
	OutPath.Add(CurrentPoint);

	FVector CurrentNormal = WallNormal;
	FVector CurrentDir    = SlideDir;
	double  Accumulated   = 0.0;

	while (Accumulated < StopDistance && OutPath.Num() < PointBudget)
	{
		const FVector ProbeOrigin = CurrentPoint + CurrentDir * StepDistance;

		// 前方被挡即到此为止：内凹角或柱子处继续采样会让折线穿墙
		FHitResult ForwardHit;
		if (World->LineTraceSingleByChannel(ForwardHit, CurrentPoint, ProbeOrigin, ECC_WorldStatic, QueryParams))
		{
			break;
		}

		// 自前进后的位置回探墙面，法线随之更新，折线因而贴合曲面而非一路直线
		FHitResult SideHit;
		if (!World->LineTraceSingleByChannel(SideHit, ProbeOrigin, ProbeOrigin - CurrentNormal * (PairSeparation + WallProbeDistance), ECC_WorldStatic, QueryParams))
		{
			break;
		}

		const FVector NextNormal = SideHit.ImpactNormal.GetSafeNormal2D();
		if (NextNormal.IsNearlyZero())
		{
			break;
		}

		FVector NextPoint = SideHit.ImpactPoint + NextNormal * PairSeparation;
		NextPoint.Z       = CurrentPoint.Z;

		const double StepLength = FVector::Distance(CurrentPoint, NextPoint);
		if (StepLength <= UE_DOUBLE_KINDA_SMALL_NUMBER)
		{
			break;
		}

		// 方向随新法线重新投影，转角处才不会切进墙里
		const FVector NextDir = (CurrentDir - CurrentDir.Dot(NextNormal) * NextNormal).GetSafeNormal2D();
		if (NextDir.IsNearlyZero())
		{
			break;
		}

		OutPath.Add(NextPoint);
		Accumulated += StepLength;

		CurrentPoint  = NextPoint;
		CurrentNormal = NextNormal;
		CurrentDir    = NextDir;
	}

	return OutPath.Num() >= 2;
}
