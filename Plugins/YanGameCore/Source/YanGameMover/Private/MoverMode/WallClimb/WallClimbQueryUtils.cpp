#include "MoverMode/WallClimb/WallClimbQueryUtils.h"

#include "Engine/World.h"
#include "Framework/Threading.h"
#include "Physics/GenericPhysicsInterface.h"

namespace UE::YanMover::WallClimb
{
	bool WallSweep_Internal(const FWallSweepParams& Params, FWallCheckResult& OutResult)
	{
		QUICK_SCOPE_CYCLE_COUNTER(YanMoverWallClimb_WallSweep);

		Chaos::EnsureIsInPhysicsThreadContext();

		OutResult = FWallCheckResult();

		if (!Params.World || Params.ProbeDir.IsNearlyZero())
		{
			return false;
		}

		// 自胶囊中心向外扫描，探测长度覆盖胶囊半径本身，使贴墙状态也能稳定命中
		const FVector Start = Params.Location;
		const FVector End   = Start + Params.ProbeDir * (Params.PawnCollisionRadius + Params.ProbeDistance);

		TArray<FHitResult> Hits;
		if (!Chaos::Private::FGenericPhysicsInterface_Internal::SpherecastMulti(Params.World, Params.ProbeRadius, Hits, Start, End, Params.CollisionChannel, Params.QueryParams, Params.ResponseParams))
		{
			return false;
		}

		// 结果已排序，阻挡命中位于末位
		const FHitResult& Hit = Hits.Last();

		OutResult.bBlockingHit = true;
		OutResult.HitResult    = Hit;
		// 起始即穿透时 ImpactNormal 不可靠，改用分离方向 Normal
		OutResult.Normal = (Hit.bStartPenetrating ? Hit.Normal : Hit.ImpactNormal).GetSafeNormal();
		// 球心停止位置距墙面为探测半径，换算成胶囊表面到墙面的距离
		OutResult.Distance = FMath::Max(Hit.Distance + Params.ProbeRadius - Params.PawnCollisionRadius, 0.f);

		return true;
	}

	bool IsClimbableAngle(const FVector& WallNormal, const FVector& UpDir, const float MinDegrees, const float MaxDegrees)
	{
		if (WallNormal.IsNearlyZero() || UpDir.IsNearlyZero())
		{
			return false;
		}

		// 墙面倾角即法线与上方向的夹角；余弦随角度单调递减，故上下限的比较方向相反
		const float NormalDotUp = FVector::DotProduct(WallNormal.GetSafeNormal(), UpDir.GetSafeNormal());
		const float MaxCosine   = FMath::Cos(FMath::DegreesToRadians(MinDegrees));
		const float MinCosine   = FMath::Cos(FMath::DegreesToRadians(MaxDegrees));

		return NormalDotUp <= MaxCosine && NormalDotUp >= MinCosine;
	}

	bool IsFacingWall(const FVector& Direction, const FVector& WallNormal, const FVector& UpDir, const float MaxDegrees)
	{
		const FVector DirectionHoriz = FVector::VectorPlaneProject(Direction, UpDir).GetSafeNormal();
		const FVector InwardHoriz    = FVector::VectorPlaneProject(-WallNormal, UpDir).GetSafeNormal();

		if (DirectionHoriz.IsNearlyZero() || InwardHoriz.IsNearlyZero())
		{
			return false;
		}

		return FVector::DotProduct(DirectionHoriz, InwardHoriz) > FMath::Cos(FMath::DegreesToRadians(MaxDegrees));
	}

	FVector ProjectOntoWallPlane(const FVector& Vector, const FVector& WallNormal)
	{
		return FVector::VectorPlaneProject(Vector, WallNormal.GetSafeNormal());
	}

	namespace Blackboard
	{
		const FName WallClimbNormal = TEXT("YanWallClimbNormal");
	}
}
