#include "MoverLayeredMove/YanWallGrindMove.h"

#include "MoverDataModelTypes.h"
#include "MoverLayeredMove/YanWallGrindPath.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "YanMoverDataModelTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanWallGrindMove)

UE_DEFINE_GAMEPLAY_TAG(Mover_IsWallGrinding, "Mover.IsWallGrinding");

namespace YanWallGrind
{
	// 限速比对的容差（cm/s）：位置差反推的速度与理论值本就有浮点级出入，不留容差会持续误判
	constexpr float SpeedErrorAllowance = 0.5f;
}

FYanLayeredMove_WallGrind::FYanLayeredMove_WallGrind()
{
	// 兜底上限而非预期时长：正常由推进走完、发起方松手或被推方挣脱终结，
	// 本值只保证一方掉线时另一方不会被永久钉住
	DurationMs = 8000.f;

	// 必须接管朝向，故不能像瞬移那样只覆盖速度
	MixMode = EMoveMixMode::OverrideAll;
}

bool FYanLayeredMove_WallGrind::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	return bExactMatch ? TagToFind.MatchesTagExact(Mover_IsWallGrinding) : TagToFind.MatchesTag(Mover_IsWallGrinding);
}

void FYanLayeredMove_WallGrind::GetGameplayTags(FGameplayTagContainer& InOutTags) const
{
	InOutTags.AddTag(Mover_IsWallGrinding);
}

bool FYanLayeredMove_WallGrind::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	return GenerateMove_Async(StartState, TimeStep, SimBlackboard, OutProposedMove);
}

bool FYanLayeredMove_WallGrind::GenerateMove_Async(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	const FMoverDefaultSyncState* SyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!SyncState)
	{
		return false;
	}

	const float DeltaSeconds = TimeStep.StepMs / 1000.f;
	if (DeltaSeconds <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	// 折线退化则本效果无从求值，立即让位给常规移动模式
	if (Path.Num() < 2)
	{
		DurationMs = 0.f;
		return false;
	}

	if (bCanBreakFree || bEndOnGrindIntentCleared)
	{
		const FYanCharacterInputs* YanInputs = StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();

		const bool bBrokeFree  = bCanBreakFree && YanInputs && YanInputs->bWantsToBreakFree;
		const bool bIntentGone = bEndOnGrindIntentCleared && (!YanInputs || !YanInputs->bWantsToWallGrind);

		if (bBrokeFree || bIntentGone)
		{
			// 时长小于已存活时长即视为结束，等价于 IsFinished 返回真。
			// 判定源自随输入包上传并参与重演的输入位，两端在同一模拟步得到相同结果
			DurationMs = 0.f;
			return false;
		}
	}

	const FVector CurrentLocation = SyncState->GetLocation_WorldSpace();

	// 缓动自本效果生效起算，起滑则等到双方共同的绝对时刻——两侧的生效时刻相差一个往返，
	// 先到的一方在悬停段等齐，双方才能同时起滑
	const double ElapsedSinceStartMs = TimeStep.BaseSimTimeMs - StartSimTimeMs;

	FVector Tangent  = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;

	if (PullDurationMs > 0.f && ElapsedSinceStartMs < PullDurationMs)
	{
		const FVector PathStartLocation = ComputeTargetLocation(0.0, Tangent);

		const float LinearFraction     = FMath::Clamp(static_cast<float>(ElapsedSinceStartMs) / PullDurationMs, 0.f, 1.f);
		const float PrevLinearFraction = FMath::Clamp(static_cast<float>(ElapsedSinceStartMs - DeltaSeconds * 1000.f) / PullDurationMs, 0.f, 1.f);

		const FVector DesiredLocation     = FMath::Lerp(StartLocation, PathStartLocation, ApplyTimeMapping(LinearFraction));
		const FVector PrevDesiredLocation = FMath::Lerp(StartLocation, PathStartLocation, ApplyTimeMapping(PrevLinearFraction));

		Velocity = ComputeTrackingVelocity(DesiredLocation, PrevDesiredLocation, CurrentLocation, DeltaSeconds);
	}
	else
	{
		const double GrindElapsedSeconds = (TimeStep.BaseSimTimeMs - GrindStartSimTimeMs) / 1000.0;

		if (GrindElapsedSeconds < 0.0)
		{
			// 悬停段：锁在折线起点抵住重力，等对侧的效果生效
			const FVector DesiredLocation = ComputeTargetLocation(0.0, Tangent);
			Velocity                      = ComputeHoldVelocity(DesiredLocation, CurrentLocation, DeltaSeconds);
		}
		else
		{
			const double TotalLength   = YanWallGrindPath::ComputeTotalLength(Path);
			const double GrindDuration = YanWallGrindPath::ComputeDuration(EntrySpeed, Deceleration, TotalLength);

			if (GrindElapsedSeconds >= GrindDuration)
			{
				// 速度耗尽或走完折线：推进自然结束，甩出被推方由服务器另行调度
				DurationMs = 0.f;
				return false;
			}

			const double TraveledDistance = YanWallGrindPath::ComputeTraveledDistance(EntrySpeed, Deceleration, GrindElapsedSeconds, TotalLength);
			const double PrevDistance     = YanWallGrindPath::ComputeTraveledDistance(EntrySpeed, Deceleration, GrindElapsedSeconds - DeltaSeconds, TotalLength);

			FVector        PrevTangent;
			const FVector  DesiredLocation     = ComputeTargetLocation(TraveledDistance, Tangent);
			const FVector  PrevDesiredLocation = ComputeTargetLocation(PrevDistance, PrevTangent);

			Velocity = ComputeTrackingVelocity(DesiredLocation, PrevDesiredLocation, CurrentLocation, DeltaSeconds);
		}
	}

	OutProposedMove.MixMode        = MixMode;
	OutProposedMove.LinearVelocity = Velocity;

	if (bControlOrientation)
	{
		OutProposedMove.AngularVelocityDegrees = ComputeAngularVelocity(SyncState->GetOrientation_WorldSpace(), Tangent, DeltaSeconds);
	}

	return true;
}

float FYanLayeredMove_WallGrind::ApplyTimeMapping(float LinearFraction) const
{
	return FMath::Pow(LinearFraction, EaseExponent);
}

FVector FYanLayeredMove_WallGrind::ComputeTargetLocation(double Distance, FVector& OutTangent) const
{
	FVector PathLocation;
	if (!YanWallGrindPath::EvaluateAtDistance(Path, Distance, PathLocation, OutTangent))
	{
		return PathLocation;
	}

	return PathLocation + YanWallGrindPath::TransformFrameOffset(OutTangent, PathFrameOffset);
}

FVector FYanLayeredMove_WallGrind::ComputeTrackingVelocity(const FVector& DesiredLocation, const FVector& PreviousDesiredLocation, const FVector& CurrentLocation, float DeltaSeconds) const
{
	// 速度由位置差反推：期望位置停止前移后，本式自然收敛到零
	FVector Velocity = (DesiredLocation - CurrentLocation) / DeltaSeconds;

	if (bRestrictSpeedToExpected && !Velocity.IsNearlyZero(UE_KINDA_SMALL_NUMBER))
	{
		// 理论速度取自「上一步期望位置到本步期望位置」，与角色实际被阻挡与否无关
		const float ExpectedSpeed = static_cast<float>(((DesiredLocation - PreviousDesiredLocation) / DeltaSeconds).Size());

		if (Velocity.SizeSquared() > FMath::Square(ExpectedSpeed + YanWallGrind::SpeedErrorAllowance))
		{
			Velocity = Velocity.GetSafeNormal() * ExpectedSpeed;
		}
	}

	return Velocity;
}

FVector FYanLayeredMove_WallGrind::ComputeHoldVelocity(const FVector& DesiredLocation, const FVector& CurrentLocation, float DeltaSeconds) const
{
	FVector Velocity = (DesiredLocation - CurrentLocation) / DeltaSeconds;

	if (MaxHoldSpeed > 0.f && Velocity.SizeSquared() > FMath::Square(MaxHoldSpeed))
	{
		Velocity = Velocity.GetSafeNormal() * MaxHoldSpeed;
	}

	return Velocity;
}

FVector FYanLayeredMove_WallGrind::ComputeAngularVelocity(const FRotator& CurrentOrientation, const FVector& Tangent, float DeltaSeconds) const
{
	// 切线退化时保持原朝向，避免零向量退化成任意角度
	if (Tangent.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	const float TargetYaw = Tangent.Rotation().Yaw + YawOffsetFromTangent;
	const float DeltaYaw  = FRotator::NormalizeAxis(TargetYaw - CurrentOrientation.Yaw);
	const float YawRate   = FMath::Clamp(DeltaYaw / DeltaSeconds, -TurnRateDegrees, TurnRateDegrees);

	return FVector(0.f, 0.f, YawRate);
}

FLayeredMoveBase* FYanLayeredMove_WallGrind::Clone() const
{
	return new FYanLayeredMove_WallGrind(*this);
}

void FYanLayeredMove_WallGrind::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	// 折线点数与输入包同一上限；本结构不参与逐字段的输入比对，故可量化传输
	uint8 NumPathPoints = Ar.IsLoading() ? 0 : static_cast<uint8>(FMath::Min(Path.Num(), YanMoverInputs::MaxWallGrindPathPoints));
	Ar << NumPathPoints;

	if (Ar.IsLoading())
	{
		Path.SetNum(NumPathPoints);
	}
	for (uint8 PointIndex = 0; PointIndex < NumPathPoints; ++PointIndex)
	{
		SerializePackedVector<10, 24>(Path[PointIndex], Ar);
	}
	if (Ar.IsLoading() && Path.Num() > YanMoverInputs::MaxWallGrindPathPoints)
	{
		// 超额的点先读完再丢弃：中途截断会让归档流错位，后续字段一并读坏
		Path.SetNum(YanMoverInputs::MaxWallGrindPathPoints);
	}

	SerializePackedVector<10, 24>(StartLocation, Ar);
	SerializePackedVector<10, 24>(PathFrameOffset, Ar);

	Ar << GrindStartSimTimeMs;
	Ar << EntrySpeed;
	Ar << Deceleration;
	Ar << PullDurationMs;
	Ar << EaseExponent;
	Ar << MaxHoldSpeed;
	Ar << YawOffsetFromTangent;
	Ar << TurnRateDegrees;

	Ar.SerializeBits(&bRestrictSpeedToExpected, 1);
	Ar.SerializeBits(&bControlOrientation, 1);
	Ar.SerializeBits(&bCanBreakFree, 1);
	Ar.SerializeBits(&bEndOnGrindIntentCleared, 1);
}

UScriptStruct* FYanLayeredMove_WallGrind::GetScriptStruct() const
{
	return FYanLayeredMove_WallGrind::StaticStruct();
}

FString FYanLayeredMove_WallGrind::ToSimpleString() const
{
	return FString::Printf(TEXT("WallGrind"));
}

void FYanLayeredMove_WallGrind::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}
