#include "YanMoverAngelscriptLibrary.h"

#include "LayeredMoveBase.h"
#include "MovementMode.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/LayeredMoves/LaunchMove.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "ChaosMover/Character/Effects/ChaosCharacterApplyVelocityEffect.h"
#include "ChaosMover/ChaosMoverActionTypes.h"
#include "UObject/UObjectIterator.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"
#include "Actor/YanHookProjectile.h"
#include "AbilitySystemComponent.h"
#include "GE/MoverLaunchExecutionCalculation.h"
#include "Clinch/YanClinchAuthorityComponent.h"
#include "Components/CapsuleComponent.h"
#include "Messages/VerbMessageHelpers.h"
#include "MoverModifier/YanMaxSpeedScaleModifier.h"
#include "MoverEffect/YanTeleportEffect.h"
#include "MoverLayeredMove/YanBlinkToMove.h"
#include "MoverLayeredMove/YanClinchMove.h"
#include "MoverLayeredMove/YanGrapplePullMove.h"
#include "MoverLayeredMove/YanNoLandingMove.h"
#include "MoverLayeredMove/YanRadialAttractionMove.h"
#include "Physics/NetworkPhysicsComponent.h"
#include "Physics/NetworkPhysicsSettingsComponent.h"
#include "PhysicsEngine/PhysicsSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanMoverAngelscriptLibrary)

namespace
{
	// sim action 的调度延迟：须大于最大 RTT，各端才能在同一物理帧应用而不触发修正
	float GetEventSchedulingDelaySeconds(AActor* Owner)
	{
		if (UNetworkPhysicsSettingsComponent* SettingsComp = Owner->FindComponentByClass<UNetworkPhysicsSettingsComponent>())
		{
			return SettingsComp->GetSettings().GeneralSettings.EventSchedulingMinDelaySeconds;
		}

		return UPhysicsSettings::Get()->PhysicsPrediction.MaxSupportedLatencyPrediction;
	}
}

bool UYanMoverAngelscriptLibrary::GetDefaultSyncState(const FMoverDataCollection& Collection, FMoverDefaultSyncState& OutSyncState)
{
	if (const FMoverDefaultSyncState* Found = Collection.FindDataByType<FMoverDefaultSyncState>())
	{
		OutSyncState = *Found;
		return true;
	}

	return false;
}

bool UYanMoverAngelscriptLibrary::GetDefaultInputs(const FMoverDataCollection& Collection, FCharacterDefaultInputs& OutInputs)
{
	if (const FCharacterDefaultInputs* Found = Collection.FindDataByType<FCharacterDefaultInputs>())
	{
		OutInputs = *Found;
		return true;
	}

	return false;
}

bool UYanMoverAngelscriptLibrary::GetYanCharacterInputs(const FMoverDataCollection& Collection, FYanCharacterInputs& OutInputs)
{
	if (const FYanCharacterInputs* Found = Collection.FindDataByType<FYanCharacterInputs>())
	{
		OutInputs = *Found;
		return true;
	}

	return false;
}

void UYanMoverAngelscriptLibrary::InitTickEndForNewFrame(FMoverTickEndData& TickEndData)
{
	TickEndData.InitForNewFrame();
}


void UYanMoverAngelscriptLibrary::ResetMovementModeTickEndState(FMovementModeTickEndState& EndState)
{
	EndState.ResetToDefaults();
}

void UYanMoverAngelscriptLibrary::ResetMovementRecord(FMovementRecord& Record)
{
	Record.Reset();
}

FMovementSubstep UYanMoverAngelscriptLibrary::MakeMovementSubstep(FName MoveName, FVector MoveDelta, bool bIsRelevant)
{
	return FMovementSubstep(MoveName, MoveDelta, bIsRelevant);
}

void UYanMoverAngelscriptLibrary::AppendMovementSubstep(FMovementRecord& Record, FMovementSubstep Substep)
{
	Record.Append(Substep);
}

FVector UYanMoverAngelscriptLibrary::GetMovementRecordTotalMoveDelta(const FMovementRecord& Record)
{
	return Record.GetTotalMoveDelta();
}

FVector UYanMoverAngelscriptLibrary::GetMovementRecordRelevantMoveDelta(const FMovementRecord& Record)
{
	return Record.GetRelevantMoveDelta();
}

FVector UYanMoverAngelscriptLibrary::GetMovementRecordRelevantVelocity(const FMovementRecord& Record)
{
	return Record.GetRelevantVelocity();
}

TArray<FMovementSubstep> UYanMoverAngelscriptLibrary::GetMovementRecordSubsteps(const FMovementRecord& Record)
{
	return TArray<FMovementSubstep>(Record.GetSubsteps());
}

void UYanMoverAngelscriptLibrary::LockMovementRecordRelevancy(FMovementRecord& Record, bool bLockValue)
{
	Record.LockRelevancy(bLockValue);
}

void UYanMoverAngelscriptLibrary::UnlockMovementRecordRelevancy(FMovementRecord& Record)
{
	Record.UnlockRelevancy();
}

void UYanMoverAngelscriptLibrary::SetMovementRecordDeltaSeconds(FMovementRecord& Record, float DeltaSeconds)
{
	Record.SetDeltaSeconds(DeltaSeconds);
}

FString UYanMoverAngelscriptLibrary::MovementRecordToString(const FMovementRecord& Record)
{
	return Record.ToString();
}

FVector UYanMoverAngelscriptLibrary::GetSyncStateVelocity_WorldSpace(const FMoverDefaultSyncState& SyncState)
{
	return SyncState.GetVelocity_WorldSpace();
}

FRotator UYanMoverAngelscriptLibrary::GetSyncStateOrientation_WorldSpace(const FMoverDefaultSyncState& SyncState)
{
	return SyncState.GetOrientation_WorldSpace();
}

bool UYanMoverAngelscriptLibrary::GetFatigueEntry(const UMoverComponent* MoverComp, EYanFatigueChannel Channel, FYanFatigueEntry& OutEntry)
{
	if (!MoverComp)
	{
		return false;
	}
	const FYanFatigueEntry* Entry = FYanFatigueState::FindEntry(MoverComp->GetSyncState(), Channel);
	if (!Entry)
	{
		return false;
	}
	OutEntry = *Entry;
	return true;
}

bool UYanMoverAngelscriptLibrary::HasWalkableFloor(UMoverComponent* MoverComp)
{
	if (!MoverComp)
	{
		return false;
	}
	const UMoverBlackboard* Blackboard = MoverComp->GetSimBlackboard();
	if (!Blackboard)
	{
		return false;
	}
	FFloorCheckResult FloorResult;
	return Blackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult) && FloorResult.IsWalkableFloor();
}

FVector UYanMoverAngelscriptLibrary::GetLastFloorNormal(UMoverComponent* MoverComp)
{
	if (!MoverComp)
	{
		return FVector::UpVector;
	}
	const UMoverBlackboard* Blackboard = MoverComp->GetSimBlackboard();
	if (!Blackboard)
	{
		return MoverComp->GetUpDirection();
	}
	FFloorCheckResult FloorResult;
	if (Blackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult) && FloorResult.IsWalkableFloor())
	{
		return FloorResult.HitResult.ImpactNormal;
	}
	return MoverComp->GetUpDirection();
}

bool UYanMoverAngelscriptLibrary::HasWalkableFloorFromBlackboard(UMoverBlackboard* SimBlackboard)
{
	if (!SimBlackboard)
	{
		return false;
	}
	UMoverComponent* MoverComp = SimBlackboard->GetTypedOuter<UMoverComponent>();
	return HasWalkableFloor(MoverComp);
}

FVector UYanMoverAngelscriptLibrary::GetMoveInputWorldSpace(const FCharacterDefaultInputs& Inputs)
{
	return Inputs.GetMoveInput_WorldSpace();
}

void UYanMoverAngelscriptLibrary::QueueChaosCharacterApplyVelocityEffect(UMoverComponent* MoverComp, FVector WorldSpaceVelocity, bool bAdditive)
{
	if (!MoverComp)
	{
		return;
	}

	TSharedPtr<FChaosCharacterApplyVelocityEffect> Effect = MakeShared<FChaosCharacterApplyVelocityEffect>();
	Effect->VelocityOrImpulseToApply                      = WorldSpaceVelocity;
	Effect->Mode                                          = bAdditive ? EChaosMoverVelocityEffectMode::AdditiveVelocity : EChaosMoverVelocityEffectMode::OverrideVelocity;
	MoverComp->QueueInstantMovementEffect(Effect);
}

FMovementModifierHandle UYanMoverAngelscriptLibrary::QueueMaxSpeedScale(UMoverComponent* MoverComp, float SpeedScale)
{
	if (!MoverComp)
	{
		return FMovementModifierHandle(MODIFIER_INVALID_HANDLE);
	}

	TSharedPtr<FYanMaxSpeedScaleModifier> Modifier = MakeShared<FYanMaxSpeedScaleModifier>();
	Modifier->SpeedScale                           = SpeedScale;
	return MoverComp->QueueMovementModifier(Modifier);
}

void UYanMoverAngelscriptLibrary::QueueApplyVelocityEffect(UMoverComponent* MoverComp, FName NewModeName)
{
	if (!MoverComp)
	{
		return;
	}
	// 零速度叠加 = 保持当前速度，ForceMovementMode 强制切换模式
	TSharedPtr<FApplyVelocityEffect> Effect = MakeShared<FApplyVelocityEffect>();
	Effect->VelocityToApply                 = FVector::ZeroVector;
	Effect->bAdditiveVelocity               = true;
	Effect->ForceMovementMode               = NewModeName;
	MoverComp->QueueInstantMovementEffect(Effect);
}

FVector UYanMoverAngelscriptLibrary::GetGravityFromBlackboard(UMoverBlackboard* SimBlackboard)
{
	if (!SimBlackboard)
	{
		return FVector(0.f, 0.f, -980.f);
	}
	// SimBlackboard 由 MoverComponent 持有，Outer 即为 MoverComponent
	if (UMoverComponent* MoverComp = SimBlackboard->GetTypedOuter<UMoverComponent>())
	{
		if (!HasWalkableFloor(MoverComp))
		{
			return MoverComp->GetGravityAcceleration();
		}
	}
	return FVector(0.f, 0.f, -980.f);
}

FVector UYanMoverAngelscriptLibrary::GetSyncStateLocation_WorldSpace(const FMoverDefaultSyncState& SyncState)
{
	return SyncState.GetLocation_WorldSpace();
}

FCharacterDefaultInputs UYanMoverAngelscriptLibrary::GetOrAddDefaultInputs(FMoverInputCmdContext& InputCmd)
{
	return InputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
}

void UYanMoverAngelscriptLibrary::CommitDefaultInputs(FMoverInputCmdContext& InputCmd, const FCharacterDefaultInputs& Inputs)
{
	FCharacterDefaultInputs& Stored = InputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	Stored                          = Inputs;
}

FYanCharacterInputs UYanMoverAngelscriptLibrary::GetOrAddYanInputs(FMoverInputCmdContext& InputCmd)
{
	return InputCmd.InputCollection.FindOrAddMutableDataByType<FYanCharacterInputs>();
}

void UYanMoverAngelscriptLibrary::CommitYanInputs(FMoverInputCmdContext& InputCmd, const FYanCharacterInputs& Inputs)
{
	FYanCharacterInputs& Stored = InputCmd.InputCollection.FindOrAddMutableDataByType<FYanCharacterInputs>();
	Stored                      = Inputs;
}

bool UYanMoverAngelscriptLibrary::GetOwnerViewLocationAndForward(UMoverComponent* MoverComp, FVector& OutLocation, FVector& OutForward)
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
	// 原点取 Pawn 视点（Actor 位置 + BaseEyeHeight），两端确定性一致，不依赖纯客户端的 PlayerCameraManager。
	OutLocation                                = Pawn->GetPawnViewLocation();
	// 方向取上传服务器的输入 ControlRotation（含完整 Pitch），两端一致；缺输入时退回 Actor 朝向。
	const FCharacterDefaultInputs* CharInputs  = MoverComp->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FRotator                 AimRotation = CharInputs ? CharInputs->ControlRotation : Pawn->GetActorRotation();
	OutForward                                 = AimRotation.Vector();
	return true;
}

bool UYanMoverAngelscriptLibrary::LineTraceForGrapple(UMoverComponent* MoverComp, float MaxDistance, FVector& OutImpactPoint)
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
	// 两端确定性一致，便于服务器侧做命中校验，不依赖纯客户端的 PlayerCameraManager。
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

bool UYanMoverAngelscriptLibrary::SweepBlinkTarget(UMoverComponent* MoverComp, FVector Direction, float MaxDistance, FVector& OutTargetLocation)
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

APawn* UYanMoverAngelscriptLibrary::FindClinchTarget(UMoverComponent* MoverComp,
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

bool UYanMoverAngelscriptLibrary::TraceWallGrindPath(UMoverComponent* MoverComp,
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

void UYanMoverAngelscriptLibrary::QueueLaunchMove(UMoverComponent* MoverComp, FVector LaunchVelocity, FName ForceMovementMode, float DurationMs)
{
	if (!MoverComp)
	{
		return;
	}

	TSharedPtr<FLayeredMove_Launch> LaunchMove = MakeShared<FLayeredMove_Launch>();
	LaunchMove->LaunchVelocity                 = LaunchVelocity;
	LaunchMove->ForceMovementMode              = ForceMovementMode;
	LaunchMove->DurationMs                     = DurationMs;
	MoverComp->QueueLayeredMove(LaunchMove);
}

void UYanMoverAngelscriptLibrary::QueueLinearVelocityMove(UMoverComponent* MoverComp, FVector Velocity, float DurationMs, bool bOverrideVelocity)
{
	if (!MoverComp)
	{
		return;
	}

	TSharedPtr<FLayeredMove_LinearVelocity> LinearMove = MakeShared<FLayeredMove_LinearVelocity>();
	LinearMove->Velocity                               = Velocity;
	LinearMove->DurationMs                             = DurationMs;
	// Override 覆盖本帧提议速度（屏蔽方向输入与摩擦，直线冲刺）；否则默认叠加到正常移动上
	LinearMove->MixMode                                = bOverrideVelocity ? EMoveMixMode::OverrideVelocity : EMoveMixMode::AdditiveVelocity;
	MoverComp->QueueLayeredMove(LinearMove);
}

AYanHookProjectile* UYanMoverAngelscriptLibrary::SpawnHookProjectile(UMoverComponent*                MoverComp,
                                                                     TSubclassOf<AYanHookProjectile> ProjectileClass,
                                                                     FVector                         SpawnLocation,
                                                                     FRotator                        SpawnRotation)
{
	if (!MoverComp || !ProjectileClass)
	{
		return nullptr;
	}
	UWorld* World = MoverComp->GetWorld();
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner                          = MoverComp->GetOwner();
	return World->SpawnActor<AYanHookProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, Params);
}

ULayeredMoveLogic* UYanMoverAngelscriptLibrary::FindRegisteredMoveLogic(UMoverComponent* MoverComp, TSubclassOf<ULayeredMoveLogic> MoveLogicClass)
{
	if (!MoverComp || !MoveLogicClass)
	{
		return nullptr;
	}
	// Logic 对象以 MoverComponent 为 Outer 创建，遍历其子对象即可找到
	TArray<UObject*> Children;
	GetObjectsWithOuter(MoverComp, Children, /*EGtObjectsFlags*/EGetObjectsFlags::None);
	for (UObject* Child : Children)
	{
		if (ULayeredMoveLogic* Logic = Cast<ULayeredMoveLogic>(Child))
		{
			if (Logic->IsA(MoveLogicClass))
			{
				return Logic;
			}
		}
	}
	return nullptr;
}

bool UYanMoverAngelscriptLibrary::ApplyAuthoritativeVelocityToTarget(UMoverComponent* MoverComp,
                                                                     FVector          WorldSpaceVelocity,
                                                                     bool             bOverrideVelocity,
                                                                     bool             bScheduleForSync)
{
	AActor* Owner = MoverComp ? MoverComp->GetOwner() : nullptr;
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	UNetworkPhysicsComponent* NetPhysics = Owner->FindComponentByClass<UNetworkPhysicsComponent>();
	if (!NetPhysics)
	{
		return false;
	}

	FChaosCharacterApplyVelocityEffect VelocityEffect;
	VelocityEffect.VelocityOrImpulseToApply = WorldSpaceVelocity;
	VelocityEffect.Mode                     = bOverrideVelocity ? EChaosMoverVelocityEffectMode::OverrideVelocity : EChaosMoverVelocityEffectMode::AdditiveVelocity;

	// Authority 风格是跨 Pawn 施力的前提：默认的 Proposed 风格会把本次效果当作目标端的提议去校验，
	// 而目标端根本没有产生过它。
	FChaosMoverInstantMovementEffectAction Action;
	Action.AuthorStyle = FNetworkPhysicsActionPayload::EActionAuthorStyle::Authority;
	Action.Effect.InitializeAsScriptStruct(FChaosCharacterApplyVelocityEffect::StaticStruct(), reinterpret_cast<const uint8*>(&VelocityEffect));

	const float DelaySeconds = bScheduleForSync ? GetEventSchedulingDelaySeconds(Owner) : 0.f;
	NetPhysics->EnqueueScheduledAction_External(Action, /*SourceId=*/0u, DelaySeconds, /*bReliable=*/true);

	return true;
}

bool UYanMoverAngelscriptLibrary::ApplyAuthoritativeTeleportToTarget(UMoverComponent* MoverComp,
                                                                     FVector          TargetLocation,
                                                                     FRotator         TargetRotation,
                                                                     bool             bOverrideVelocity,
                                                                     FVector          VelocityAfterTeleport,
                                                                     bool             bScheduleForSync)
{
	AActor* Owner = MoverComp ? MoverComp->GetOwner() : nullptr;
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	UNetworkPhysicsComponent* NetPhysics = Owner->FindComponentByClass<UNetworkPhysicsComponent>();
	if (!NetPhysics)
	{
		return false;
	}

	FYanTeleportEffect TeleportEffect;
	TeleportEffect.TargetLocation                 = TargetLocation;
	TeleportEffect.TargetRotation                 = TargetRotation;
	TeleportEffect.bUseActorRotation              = false;
	TeleportEffect.bOverrideVelocityAfterTeleport = bOverrideVelocity;
	TeleportEffect.VelocityAfterTeleport          = VelocityAfterTeleport;

	// 同 ApplyAuthoritativeVelocityToTarget：传送由世界 Actor 发起，目标端没有产生过它
	FChaosMoverInstantMovementEffectAction Action;
	Action.AuthorStyle = FNetworkPhysicsActionPayload::EActionAuthorStyle::Authority;
	Action.Effect.InitializeAsScriptStruct(FYanTeleportEffect::StaticStruct(), reinterpret_cast<const uint8*>(&TeleportEffect));

	const float DelaySeconds = bScheduleForSync ? GetEventSchedulingDelaySeconds(Owner) : 0.f;
	NetPhysics->EnqueueScheduledAction_External(Action, /*SourceId=*/0u, DelaySeconds, /*bReliable=*/true);

	return true;
}

bool UYanMoverAngelscriptLibrary::ApplyBlinkToTarget(UMoverComponent* MoverComp, FVector TargetLocation, float DurationMs, float EaseExponent, FName ForceMovementMode)
{
	AActor* Owner = MoverComp ? MoverComp->GetOwner() : nullptr;
	if (!Owner || !Owner->HasAuthority() || DurationMs <= 0.f)
	{
		return false;
	}

	UNetworkPhysicsComponent* NetPhysics = Owner->FindComponentByClass<UNetworkPhysicsComponent>();
	if (!NetPhysics)
	{
		return false;
	}

	FYanLayeredMove_BlinkTo Move;
	Move.StartLocation     = MoverComp->GetUpdatedComponentTransform().GetLocation();
	Move.TargetLocation    = TargetLocation;
	Move.DurationMs        = DurationMs;
	Move.EaseExponent      = EaseExponent;
	Move.ForceMovementMode = ForceMovementMode;

	// 同 ApplyRadialAttractionToTarget：位移作用于他人，默认的 Proposed 风格
	// 会把它当作目标端的提议去校验，而目标端根本没有产生过它
	FChaosMoverLayeredMoveAction Action;
	Action.AuthorStyle = FNetworkPhysicsActionPayload::EActionAuthorStyle::Authority;
	Action.Move.InitializeAsScriptStruct(FYanLayeredMove_BlinkTo::StaticStruct(), reinterpret_cast<const uint8*>(&Move));

	NetPhysics->EnqueueScheduledAction_External(Action, /*SourceId=*/0u, GetEventSchedulingDelaySeconds(Owner), /*bReliable=*/true);

	return true;
}

bool UYanMoverAngelscriptLibrary::ApplyRadialAttractionToTarget(UMoverComponent* MoverComp,
                                                                FVector          AttractionCenter,
                                                                float            Radius,
                                                                float            Magnitude,
                                                                float            FalloffExponent,
                                                                float            DurationMs,
                                                                bool             bIsPush)
{
	AActor* Owner = MoverComp ? MoverComp->GetOwner() : nullptr;
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	UNetworkPhysicsComponent* NetPhysics = Owner->FindComponentByClass<UNetworkPhysicsComponent>();
	if (!NetPhysics)
	{
		return false;
	}

	FYanLayeredMove_RadialAttraction Move;
	Move.Location        = AttractionCenter;
	Move.Radius          = Radius;
	Move.Magnitude       = Magnitude;
	Move.FalloffExponent = FalloffExponent;
	Move.bIsPush         = bIsPush;
	Move.DurationMs      = DurationMs;

	// 同 ApplyAuthoritativeVelocityToTarget：牵引作用于他人，默认的 Proposed 风格
	// 会把它当作目标端的提议去校验，而目标端根本没有产生过它
	FChaosMoverLayeredMoveAction Action;
	Action.AuthorStyle = FNetworkPhysicsActionPayload::EActionAuthorStyle::Authority;
	Action.Move.InitializeAsScriptStruct(FYanLayeredMove_RadialAttraction::StaticStruct(), reinterpret_cast<const uint8*>(&Move));

	NetPhysics->EnqueueScheduledAction_External(Action, /*SourceId=*/0u, GetEventSchedulingDelaySeconds(Owner), /*bReliable=*/true);

	return true;
}

bool UYanMoverAngelscriptLibrary::RefreshRadialAttractionToTarget(UMoverComponent* MoverComp, const FYanLayeredMove_RadialAttraction& Settings, bool bScheduleForSync)
{
	AActor* Owner = MoverComp ? MoverComp->GetOwner() : nullptr;
	if (!Owner || !Owner->HasAuthority() || Settings.SourceId == 0 || Settings.UpdateSequence == 0 || (Settings.Radius > 0.0f && Settings.DurationMs <= 0.0f))
	{
		return false;
	}

	UNetworkPhysicsComponent* NetPhysics = Owner->FindComponentByClass<UNetworkPhysicsComponent>();
	if (!NetPhysics)
	{
		return false;
	}

	FYanRadialAttractionUpdateEffect Update;
	Update.Settings = Settings;
	const FMoverTimeStep& LastTimeStep = MoverComp->GetLastTimeStep();
	Update.Settings.SnapshotSimTimeMs = LastTimeStep.BaseSimTimeMs + LastTimeStep.StepMs;

	FChaosMoverInstantMovementEffectAction Action;
	Action.AuthorStyle = FNetworkPhysicsActionPayload::EActionAuthorStyle::Authority;
	Action.Effect.InitializeAsScriptStruct(FYanRadialAttractionUpdateEffect::StaticStruct(), reinterpret_cast<const uint8*>(&Update));
	const float DelaySeconds = bScheduleForSync ? GetEventSchedulingDelaySeconds(Owner) : 0.0f;
	NetPhysics->EnqueueScheduledAction_External(Action, static_cast<uint32>(Settings.SourceId), DelaySeconds, /*bReliable=*/true);
	return true;
}

bool UYanMoverAngelscriptLibrary::ApplyGrapplePullToTarget(UMoverComponent* MoverComp,
                                                           FVector          MeetPoint,
                                                           float            PullAcceleration,
                                                           float            MaxPullSpeed,
                                                           float            StopDistance,
                                                           float            DurationMs)
{
	AActor* Owner = MoverComp ? MoverComp->GetOwner() : nullptr;
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	UNetworkPhysicsComponent* NetPhysics = Owner->FindComponentByClass<UNetworkPhysicsComponent>();
	if (!NetPhysics)
	{
		return false;
	}

	FYanLayeredMove_GrapplePull Move;
	Move.MeetPoint        = MeetPoint;
	Move.PullAcceleration = PullAcceleration;
	Move.MaxPullSpeed     = MaxPullSpeed;
	Move.StopDistance     = StopDistance;
	Move.DurationMs       = DurationMs;

	// 同 ApplyRadialAttractionToTarget：牵引作用于他人，默认的 Proposed 风格
	// 会把它当作目标端的提议去校验，而目标端根本没有产生过它
	FChaosMoverLayeredMoveAction Action;
	Action.AuthorStyle = FNetworkPhysicsActionPayload::EActionAuthorStyle::Authority;
	Action.Move.InitializeAsScriptStruct(FYanLayeredMove_GrapplePull::StaticStruct(), reinterpret_cast<const uint8*>(&Move));

	NetPhysics->EnqueueScheduledAction_External(Action, /*SourceId=*/0u, GetEventSchedulingDelaySeconds(Owner), /*bReliable=*/true);

	return true;
}

bool UYanMoverAngelscriptLibrary::ApplyNoLandingToTarget(UMoverComponent* MoverComp, float DurationSeconds)
{
	AActor* Owner = MoverComp ? MoverComp->GetOwner() : nullptr;
	if (!Owner || !Owner->HasAuthority() || DurationSeconds <= 0.f)
	{
		return false;
	}

	UNetworkPhysicsComponent* NetPhysics = Owner->FindComponentByClass<UNetworkPhysicsComponent>();
	if (!NetPhysics)
	{
		return false;
	}

	FYanLayeredMove_NoLanding Move;
	Move.DurationMs = DurationSeconds * 1000.f;

	// 同 ApplyAuthoritativeVelocityToTarget：效果作用于他人，默认的 Proposed 风格
	// 会把它当作目标端的提议去校验，而目标端根本没有产生过它
	FChaosMoverLayeredMoveAction Action;
	Action.AuthorStyle = FNetworkPhysicsActionPayload::EActionAuthorStyle::Authority;
	Action.Move.InitializeAsScriptStruct(FYanLayeredMove_NoLanding::StaticStruct(), reinterpret_cast<const uint8*>(&Move));

	NetPhysics->EnqueueScheduledAction_External(Action, /*SourceId=*/0u, GetEventSchedulingDelaySeconds(Owner), /*bReliable=*/true);

	return true;
}

bool UYanMoverAngelscriptLibrary::ApplyClinchToTarget(UMoverComponent* MoverComp,
                                                      FVector          AnchorLocation,
                                                      double           FallStartSimTimeMs,
                                                      float            TargetYaw,
                                                      float            MaxHoldSpeed,
                                                      float            TurnRateDegrees,
                                                      float            MaxDurationSeconds)
{
	AActor* Owner = MoverComp ? MoverComp->GetOwner() : nullptr;
	if (!Owner || !Owner->HasAuthority() || MaxDurationSeconds <= 0.f)
	{
		return false;
	}

	UNetworkPhysicsComponent* NetPhysics = Owner->FindComponentByClass<UNetworkPhysicsComponent>();
	if (!NetPhysics)
	{
		return false;
	}

	FYanLayeredMove_Clinch Move;
	// 被抓方原地锁定，起点与落位点重合，缓动段因而退化为零
	Move.StartLocation             = AnchorLocation;
	Move.AnchorLocation            = AnchorLocation;
	Move.PullDurationMs            = 0.f;
	Move.FallStartSimTimeMs        = FallStartSimTimeMs;
	// 重力在 game thread 取定并随效果下发，与发起方那一份同源，双方的下落解算据此一致
	Move.Gravity                   = MoverComp->GetGravityAcceleration();
	Move.MaxHoldSpeed              = MaxHoldSpeed;
	Move.TargetYaw                 = TargetYaw;
	Move.TurnRateDegrees           = TurnRateDegrees;
	Move.bControlOrientation       = true;
	Move.bCanBreakFree             = true;
	// 被抓方从不写抓取意图，若据此判定会一诞生就自我终结
	Move.bEndOnClinchIntentCleared = false;
	Move.DurationMs                = MaxDurationSeconds * 1000.f;

	// 同 ApplyAuthoritativeVelocityToTarget：效果作用于他人，默认的 Proposed 风格
	// 会把它当作目标端的提议去校验，而目标端根本没有产生过它
	FChaosMoverLayeredMoveAction Action;
	Action.AuthorStyle = FNetworkPhysicsActionPayload::EActionAuthorStyle::Authority;
	Action.Move.InitializeAsScriptStruct(FYanLayeredMove_Clinch::StaticStruct(), reinterpret_cast<const uint8*>(&Move));

	NetPhysics->EnqueueScheduledAction_External(Action, /*SourceId=*/0u, GetEventSchedulingDelaySeconds(Owner), /*bReliable=*/true);

	return true;
}

void UYanMoverAngelscriptLibrary::ReleaseGrabbedState(AActor* TargetActor)
{
	UAbilitySystemComponent* TargetASC = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(TargetActor);
	if (!TargetASC)
	{
		return;
	}

	FGameplayTagContainer GrantedTags;
	GrantedTags.AddTag(Status_Grabbed);
	TargetASC->RemoveActiveEffectsWithGrantedTags(GrantedTags);
}

void UYanMoverAngelscriptLibrary::ApplyLaunchEffectToTarget(UAbilitySystemComponent*     InstigatorASC,
                                                            UAbilitySystemComponent*     TargetASC,
                                                            TSubclassOf<UGameplayEffect> LaunchEffectClass,
                                                            FVector                      LaunchVelocity)
{
	if (!InstigatorASC || !TargetASC || !LaunchEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle Context = InstigatorASC->MakeEffectContext();
	Context.AddInstigator(InstigatorASC->GetAvatarActor(), InstigatorASC->GetAvatarActor());

	FGameplayEffectSpecHandle Spec = InstigatorASC->MakeOutgoingSpec(LaunchEffectClass, 1.f, Context);
	if (!Spec.IsValid())
	{
		return;
	}

	// 将 FVector 拆成 3 个浮点 SetByCaller，规避 GE Spec 无法直接携带向量的限制
	Spec.Data->SetSetByCallerMagnitude(NAME_Mover_Launch_VelocityX, LaunchVelocity.X);
	Spec.Data->SetSetByCallerMagnitude(NAME_Mover_Launch_VelocityY, LaunchVelocity.Y);
	Spec.Data->SetSetByCallerMagnitude(NAME_Mover_Launch_VelocityZ, LaunchVelocity.Z);

	InstigatorASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
}
