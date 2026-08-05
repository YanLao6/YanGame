#include "MoverMode/WallClimb/YanWallClimbState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanWallClimbState)

FMoverDataStructBase* FYanWallClimbState::Clone() const
{
	return new FYanWallClimbState(*this);
}

bool FYanWallClimbState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	Ar << ClimbedTimeMs;
	Ar << WallNormal;
	Ar << LastUpdateServerFrame;
	bOutSuccess = true;
	return true;
}

UScriptStruct* FYanWallClimbState::GetScriptStruct() const
{
	return StaticStruct();
}

bool FYanWallClimbState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FYanWallClimbState& Auth = static_cast<const FYanWallClimbState&>(AuthorityState);
	// 容差取一个物理子步的量级：浮点累加顺序差异与法线微抖不应触发回滚，真正的起始帧错位或换墙才应触发
	constexpr float ReconcileToleranceMs     = 1.f;
	constexpr float NormalReconcileTolerance = 0.05f;
	return !FMath::IsNearlyEqual(Auth.ClimbedTimeMs, ClimbedTimeMs, ReconcileToleranceMs)
	       || Auth.LastUpdateServerFrame != LastUpdateServerFrame
	       || !Auth.WallNormal.Equals(WallNormal, NormalReconcileTolerance);
}

void FYanWallClimbState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	const FYanWallClimbState& FromState = static_cast<const FYanWallClimbState&>(From);
	const FYanWallClimbState& ToState   = static_cast<const FYanWallClimbState&>(To);
	ClimbedTimeMs                       = FMath::Lerp(FromState.ClimbedTimeMs, ToState.ClimbedTimeMs, Pct);
	// 法线插值后重新归一化；两端法线接近相反时结果退化，此时保留目标端法线
	const FVector LerpedNormal          = FMath::Lerp(FromState.WallNormal, ToState.WallNormal, Pct);
	WallNormal                          = LerpedNormal.IsNearlyZero() ? ToState.WallNormal : LerpedNormal.GetSafeNormal();
	// 帧号无插值语义，按 Pct 取离散值
	LastUpdateServerFrame               = (Pct < 0.5f) ? FromState.LastUpdateServerFrame : ToState.LastUpdateServerFrame;
}

void FYanWallClimbState::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);
	Out.Appendf("ClimbedTimeMs: %.1f | WallNormal: (%.2f,%.2f,%.2f) | LastUpdateServerFrame: %i\n",
	            ClimbedTimeMs, WallNormal.X, WallNormal.Y, WallNormal.Z, LastUpdateServerFrame);
}
