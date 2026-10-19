#include "MoverLayeredMove/YanBlinkToMove.h"

#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanBlinkToMove)

namespace YanBlinkTo
{
	// 限速比对的容差（cm/s）：位置差反推的速度与理论值本就有浮点级出入，不留容差会持续误判
	constexpr float SpeedErrorAllowance = 0.5f;
}

FYanLayeredMove_BlinkTo::FYanLayeredMove_BlinkTo()
{
	DurationMs = 150.f;

	// 覆盖而非叠加：瞬移期间重力与方向输入都不该影响轨迹，否则终点位置不再可控。
	// 用 OverrideVelocity 而非 OverrideAll，是为了保留移动模式对朝向的处理。
	MixMode = EMoveMixMode::OverrideVelocity;

	// 抵达即停的最后一道保证：效果移除的那一帧由状态机把提议速度直接置零，
	// 否则末帧残留的速度会被移动模式接管，表现回退成冲量式的滑出。
	FinishVelocitySettings.FinishVelocityMode = ELayeredMoveFinishVelocityMode::SetVelocity;
	FinishVelocitySettings.SetVelocity        = FVector::ZeroVector;
}

bool FYanLayeredMove_BlinkTo::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	return GenerateMove_Async(StartState, TimeStep, SimBlackboard, OutProposedMove);
}

bool FYanLayeredMove_BlinkTo::GenerateMove_Async(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	const FMoverDefaultSyncState* SyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!SyncState || DurationMs <= 0.f)
	{
		return false;
	}

	const float DeltaSeconds = TimeStep.StepMs / 1000.f;
	if (DeltaSeconds <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float ElapsedMs      = static_cast<float>(TimeStep.BaseSimTimeMs - StartSimTimeMs);
	const float LinearFraction = FMath::Clamp(ElapsedMs / DurationMs, 0.f, 1.f);

	const FVector DesiredLocation = FMath::Lerp(StartLocation, TargetLocation, ApplyTimeMapping(LinearFraction));
	const FVector CurrentLocation = SyncState->GetLocation_WorldSpace();

	// 速度由位置差反推：期望位置停止前移后，本式自然收敛到零
	FVector Velocity = (DesiredLocation - CurrentLocation) / DeltaSeconds;

	if (bRestrictSpeedToExpected && !Velocity.IsNearlyZero(UE_KINDA_SMALL_NUMBER))
	{
		// 理论速度取自「上一步期望位置到本步期望位置」，与角色实际被阻挡与否无关
		const float   PrevLinearFraction = FMath::Clamp((ElapsedMs - TimeStep.StepMs) / DurationMs, 0.f, 1.f);
		const FVector PrevDesiredLocation = FMath::Lerp(StartLocation, TargetLocation, ApplyTimeMapping(PrevLinearFraction));
		const float   ExpectedSpeed       = ((DesiredLocation - PrevDesiredLocation) / DeltaSeconds).Size();

		if (Velocity.SizeSquared() > FMath::Square(ExpectedSpeed + YanBlinkTo::SpeedErrorAllowance))
		{
			Velocity = Velocity.GetSafeNormal() * ExpectedSpeed;
		}
	}

	OutProposedMove.MixMode        = MixMode;
	OutProposedMove.LinearVelocity = Velocity;
	OutProposedMove.PreferredMode  = ForceMovementMode;

	return true;
}

float FYanLayeredMove_BlinkTo::ApplyTimeMapping(float LinearFraction) const
{
	return FMath::Pow(LinearFraction, EaseExponent);
}

FLayeredMoveBase* FYanLayeredMove_BlinkTo::Clone() const
{
	return new FYanLayeredMove_BlinkTo(*this);
}

void FYanLayeredMove_BlinkTo::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	SerializePackedVector<10, 24>(StartLocation, Ar);
	SerializePackedVector<10, 24>(TargetLocation, Ar);

	Ar << EaseExponent;
	Ar.SerializeBits(&bRestrictSpeedToExpected, 1);

	bool bUsingForcedMovementMode = !ForceMovementMode.IsNone();
	Ar.SerializeBits(&bUsingForcedMovementMode, 1);
	if (bUsingForcedMovementMode)
	{
		Ar << ForceMovementMode;
	}
}

UScriptStruct* FYanLayeredMove_BlinkTo::GetScriptStruct() const
{
	return FYanLayeredMove_BlinkTo::StaticStruct();
}

FString FYanLayeredMove_BlinkTo::ToSimpleString() const
{
	return FString::Printf(TEXT("BlinkTo"));
}

void FYanLayeredMove_BlinkTo::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}
