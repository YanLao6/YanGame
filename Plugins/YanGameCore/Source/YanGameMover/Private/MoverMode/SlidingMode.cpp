#include "MoverMode/SlidingMode.h"

#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MoverBlackboard.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SlidingMode)

USlidingMode::USlidingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;
}

void USlidingMode::GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	// Read prior velocity from sync state; bail if unavailable
	const FMoverDefaultSyncState* SyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!SyncState)
	{
		return;
	}

	FVector     PriorVelocity = SyncState->GetVelocity_WorldSpace();
	const float DeltaSeconds  = TimeStep.StepMs / 1000.f;
	const float PriorSpeed    = PriorVelocity.Size();

	// 先查一次地面：既用于斜面判定（选摩擦系数），也用于沿坡重力加速
	bool    bOnSlope    = false;
	FVector SlopeGravity = FVector::ZeroVector;
	if (UMoverComponent* MoverComp = GetMoverComponent())
	{
		if (const UMoverBlackboard* Blackboard = MoverComp->GetSimBlackboard())
		{
			FFloorCheckResult FloorResult;
			if (Blackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult) && FloorResult.IsWalkableFloor())
			{
				const FVector FloorNormal  = FloorResult.HitResult.ImpactNormal.GetSafeNormal();
				const FVector GravityAccel = MoverComp->GetGravityAcceleration();
				const FVector UpDir        = (-GravityAccel).GetSafeNormal();

				// 法线与上方向夹角超过阈值即视为斜面（cos 单调递减，故用小于号比较）
				if (!UpDir.IsNearlyZero())
				{
					const float CosThreshold = FMath::Cos(FMath::DegreesToRadians(SlopeAngleThreshold));
					bOnSlope = FVector::DotProduct(FloorNormal, UpDir) < CosThreshold;
				}

				// 将重力投影到地面切平面，得到沿坡向下的加速度
				SlopeGravity = GravityAccel - FloorNormal * FVector::DotProduct(GravityAccel, FloorNormal);
			}
		}
	}

	// 斜面上按"移动方向与下坡方向的夹角"在下坡/上坡摩擦间插值：正对下坡取 DownhillFriction，
	// 正对上坡取 UphillFriction，沿等高线横向移动取两者中值；平地则用 SlideFriction
	float ActiveFriction = SlideFriction;
	if (bOnSlope && PriorSpeed > 0.f)
	{
		const FVector DownhillDir = SlopeGravity.GetSafeNormal();
		const FVector MoveDir     = PriorVelocity.GetSafeNormal();
		// Alpha：0 = 正对上坡，1 = 正对下坡
		const float Alpha = (FVector::DotProduct(MoveDir, DownhillDir) + 1.f) * 0.5f;
		ActiveFriction    = FMath::Lerp(UphillFriction, DownhillFriction, Alpha);
	}

	// Friction decay: dV = Friction * |V| * dt (only decelerate, never reverse)
	if (PriorSpeed > 0.f)
	{
		const float NewSpeed = FMath::Max(PriorSpeed - ActiveFriction * PriorSpeed * DeltaSeconds, 0.f);
		PriorVelocity        = PriorVelocity.GetSafeNormal() * NewSpeed;
	}

	// Slope gravity: accelerate downhill along the floor tangent plane
	if (!SlopeGravity.IsNearlyZero(0.01f))
	{
		PriorVelocity += SlopeGravity * DeltaSeconds;
	}

	OutProposedMove.bHasDirIntent          = false;
	OutProposedMove.LinearVelocity         = PriorVelocity;
	OutProposedMove.AngularVelocityDegrees = FVector::ZeroVector;
}
