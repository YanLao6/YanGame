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

	// Friction decay: dV = Friction * |V| * dt (only decelerate, never reverse)
	if (PriorSpeed > 0.f)
	{
		const float NewSpeed = FMath::Max(PriorSpeed - SlideFriction * PriorSpeed * DeltaSeconds, 0.f);
		PriorVelocity        = PriorVelocity.GetSafeNormal() * NewSpeed;
	}

	// Slope gravity: project gravity onto the floor tangent plane so the character accelerates downhill
	if (UMoverComponent* MoverComp = GetMoverComponent())
	{
		if (const UMoverBlackboard* Blackboard = MoverComp->GetSimBlackboard())
		{
			FFloorCheckResult FloorResult;
			if (Blackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult) && FloorResult.IsWalkableFloor())
			{
				const FVector FloorNormal  = FloorResult.HitResult.ImpactNormal.GetSafeNormal();
				const FVector GravityAccel = MoverComp->GetGravityAcceleration();
				// Remove the normal component, leaving only the tangential (downhill) acceleration
				const FVector SlopeGravity = GravityAccel - FloorNormal * FVector::DotProduct(GravityAccel, FloorNormal);
				if (!SlopeGravity.IsNearlyZero(0.01f))
				{
					PriorVelocity += SlopeGravity * DeltaSeconds;
				}
			}
		}
	}

	OutProposedMove.bHasDirIntent          = false;
	OutProposedMove.LinearVelocity         = PriorVelocity;
	OutProposedMove.AngularVelocityDegrees = FVector::ZeroVector;
}
