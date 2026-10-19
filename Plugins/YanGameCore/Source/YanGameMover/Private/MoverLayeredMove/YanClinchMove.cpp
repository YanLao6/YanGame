#include "MoverLayeredMove/YanClinchMove.h"

#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "YanMoverDataModelTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanClinchMove)

UE_DEFINE_GAMEPLAY_TAG(Mover_IsClinched, "Mover.IsClinched");

namespace YanClinch
{
	// 限速比对的容差（cm/s）：位置差反推的速度与理论值本就有浮点级出入，不留容差会持续误判
	constexpr float SpeedErrorAllowance = 0.5f;
}

FYanLayeredMove_Clinch::FYanLayeredMove_Clinch()
{
	// 兜底上限而非预期时长：正常由抓取 Ability 结束或被抓方挣脱终结，
	// 本值只保证一方掉线时另一方不会被永久钉住
	DurationMs = 8000.f;

	// 必须接管朝向，故不能像瞬移那样只覆盖速度
	MixMode = EMoveMixMode::OverrideAll;
}

bool FYanLayeredMove_Clinch::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	return bExactMatch ? TagToFind.MatchesTagExact(Mover_IsClinched) : TagToFind.MatchesTag(Mover_IsClinched);
}

void FYanLayeredMove_Clinch::GetGameplayTags(FGameplayTagContainer& InOutTags) const
{
	InOutTags.AddTag(Mover_IsClinched);
}

bool FYanLayeredMove_Clinch::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	return GenerateMove_Async(StartState, TimeStep, SimBlackboard, OutProposedMove);
}

bool FYanLayeredMove_Clinch::GenerateMove_Async(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
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

	if (bCanBreakFree || bEndOnClinchIntentCleared)
	{
		const FYanCharacterInputs* YanInputs = StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();

		const bool bBrokeFree   = bCanBreakFree && YanInputs && YanInputs->bWantsToBreakFree;
		const bool bIntentGone  = bEndOnClinchIntentCleared && (!YanInputs || !YanInputs->bWantsToClinch);

		if (bBrokeFree || bIntentGone)
		{
			// 时长小于已存活时长即视为结束，等价于 IsFinished 返回真。
			// 判定源自随输入包上传并参与重演的输入位，两端在同一模拟步得到相同结果
			DurationMs = 0.f;
			return false;
		}
	}

	const FVector CurrentLocation = SyncState->GetLocation_WorldSpace();

	// 缓动自本效果生效起算，下落则等到双方共同的绝对时刻——两侧的生效时刻相差一个往返，
	// 先到的一方在悬停段等齐，双方才能同时起落
	const double ElapsedSinceStartMs = TimeStep.BaseSimTimeMs - StartSimTimeMs;

	FVector Velocity;
	if (PullDurationMs > 0.f && ElapsedSinceStartMs < PullDurationMs)
	{
		Velocity = ComputePullVelocity(CurrentLocation, ElapsedSinceStartMs, DeltaSeconds);
	}
	else
	{
		const bool bAllowFall = TimeStep.BaseSimTimeMs >= FallStartSimTimeMs;
		Velocity              = ComputeHoldVelocity(CurrentLocation, SyncState->GetVelocity_WorldSpace(), DeltaSeconds, bAllowFall);
	}

	OutProposedMove.MixMode        = MixMode;
	OutProposedMove.LinearVelocity = Velocity;

	if (bControlOrientation)
	{
		OutProposedMove.AngularVelocityDegrees = ComputeAngularVelocity(SyncState->GetOrientation_WorldSpace(), DeltaSeconds);
	}

	return true;
}

float FYanLayeredMove_Clinch::ApplyTimeMapping(float LinearFraction) const
{
	return FMath::Pow(LinearFraction, EaseExponent);
}

FVector FYanLayeredMove_Clinch::ComputePullVelocity(const FVector& CurrentLocation, double ElapsedFromPullStartMs, float DeltaSeconds) const
{
	const float   LinearFraction  = FMath::Clamp(static_cast<float>(ElapsedFromPullStartMs) / PullDurationMs, 0.f, 1.f);
	const FVector DesiredLocation = FMath::Lerp(StartLocation, AnchorLocation, ApplyTimeMapping(LinearFraction));

	// 速度由位置差反推：期望位置停止前移后，本式自然收敛到零
	FVector Velocity = (DesiredLocation - CurrentLocation) / DeltaSeconds;

	if (bRestrictSpeedToExpected && !Velocity.IsNearlyZero(UE_KINDA_SMALL_NUMBER))
	{
		// 理论速度取自「上一步期望位置到本步期望位置」，与角色实际被阻挡与否无关
		const float   PrevLinearFraction  = FMath::Clamp(static_cast<float>(ElapsedFromPullStartMs - DeltaSeconds * 1000.f) / PullDurationMs, 0.f, 1.f);
		const FVector PrevDesiredLocation = FMath::Lerp(StartLocation, AnchorLocation, ApplyTimeMapping(PrevLinearFraction));
		const float   ExpectedSpeed       = ((DesiredLocation - PrevDesiredLocation) / DeltaSeconds).Size();

		if (Velocity.SizeSquared() > FMath::Square(ExpectedSpeed + YanClinch::SpeedErrorAllowance))
		{
			Velocity = Velocity.GetSafeNormal() * ExpectedSpeed;
		}
	}

	return Velocity;
}

FVector FYanLayeredMove_Clinch::ComputeHoldVelocity(const FVector& CurrentLocation, const FVector& CurrentVelocity, float DeltaSeconds, bool bAllowFall) const
{
	// 重力方向定义「垂直」，无重力时退化为完全钉死在落位点
	const FVector GravityDir = Gravity.GetSafeNormal();

	const FVector ToAnchor        = AnchorLocation - CurrentLocation;
	const double  VerticalOffset  = ToAnchor.Dot(GravityDir);
	const FVector HorizontalDelta = ToAnchor - VerticalOffset * GravityDir;

	FVector HorizontalVelocity = HorizontalDelta / DeltaSeconds;
	if (MaxHoldSpeed > 0.f && HorizontalVelocity.SizeSquared() > FMath::Square(MaxHoldSpeed))
	{
		HorizontalVelocity = HorizontalVelocity.GetSafeNormal() * MaxHoldSpeed;
	}

	FVector VerticalVelocity;
	if (bAllowFall)
	{
		// 沿重力方向的分量原样保留并累加重力：抓取双方执行同一运算序列，
		// 浮点误差随之相同，高度差因而不随时间漂移。
		// 落地无须显式判定——地面挡住位移后本项自然归零
		VerticalVelocity = CurrentVelocity.Dot(GravityDir) * GravityDir + Gravity * DeltaSeconds;
	}
	else
	{
		// 悬停段同样由位置差反推，落位点高度不变即抵住了重力
		double VerticalSpeed = VerticalOffset / DeltaSeconds;
		if (MaxHoldSpeed > 0.f)
		{
			VerticalSpeed = FMath::Clamp(VerticalSpeed, -static_cast<double>(MaxHoldSpeed), static_cast<double>(MaxHoldSpeed));
		}
		VerticalVelocity = VerticalSpeed * GravityDir;
	}

	return HorizontalVelocity + VerticalVelocity;
}

FVector FYanLayeredMove_Clinch::ComputeAngularVelocity(const FRotator& CurrentOrientation, float DeltaSeconds) const
{
	const float DeltaYaw = FRotator::NormalizeAxis(TargetYaw - CurrentOrientation.Yaw);
	const float YawRate  = FMath::Clamp(DeltaYaw / DeltaSeconds, -TurnRateDegrees, TurnRateDegrees);

	return FVector(0.f, 0.f, YawRate);
}

FLayeredMoveBase* FYanLayeredMove_Clinch::Clone() const
{
	return new FYanLayeredMove_Clinch(*this);
}

void FYanLayeredMove_Clinch::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	SerializePackedVector<10, 24>(StartLocation, Ar);
	SerializePackedVector<10, 24>(AnchorLocation, Ar);

	Ar << FallStartSimTimeMs;
	Ar << Gravity;
	Ar << PullDurationMs;
	Ar << EaseExponent;
	Ar << MaxHoldSpeed;
	Ar << TargetYaw;
	Ar << TurnRateDegrees;

	Ar.SerializeBits(&bRestrictSpeedToExpected, 1);
	Ar.SerializeBits(&bControlOrientation, 1);
	Ar.SerializeBits(&bCanBreakFree, 1);
	Ar.SerializeBits(&bEndOnClinchIntentCleared, 1);
}

UScriptStruct* FYanLayeredMove_Clinch::GetScriptStruct() const
{
	return FYanLayeredMove_Clinch::StaticStruct();
}

FString FYanLayeredMove_Clinch::ToSimpleString() const
{
	return FString::Printf(TEXT("Clinch"));
}

void FYanLayeredMove_Clinch::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}
