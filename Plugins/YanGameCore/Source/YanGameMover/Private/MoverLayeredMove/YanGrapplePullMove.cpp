#include "MoverLayeredMove/YanGrapplePullMove.h"

#include "MoverMode/Grappling/ChaosGrapplingMode.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanGrapplePullMove)

FYanLayeredMove_GrapplePull::FYanLayeredMove_GrapplePull()
{
	DurationMs = 200.0f;

	// 覆盖而非叠加：被钩者自身的下落与地面摩擦会把牵引吃掉，全额重力也会盖过减半后的钩锁重力
	MixMode = EMoveMixMode::OverrideVelocity;
}

bool FYanLayeredMove_GrapplePull::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	return GenerateMove_Async(StartState, TimeStep, SimBlackboard, OutProposedMove);
}

bool FYanLayeredMove_GrapplePull::GenerateMove_Async(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	const FMoverDefaultSyncState* SyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!SyncState)
	{
		return false;
	}

	const float   DeltaSeconds = TimeStep.StepMs * 0.001f;
	const FVector CurrentPos   = SyncState->GetLocation_WorldSpace();
	FVector       Velocity     = SyncState->GetVelocity_WorldSpace();

	const FVector ToMeetPoint = MeetPoint - CurrentPos;
	const float   Distance    = ToMeetPoint.Size();

	// 已经到位就交还控制权：继续施力只会让相向而来的双方隔着汇合点反复对冲
	if (Distance <= StopDistance)
	{
		return false;
	}

	// 重力方向取世界向上：本结构在物理线程求值，拿不到仿真输入里的 UpDir。
	// 大小与发起者侧共用 YanGrapple::GrapplingGravityMagnitude，两侧弧线才对称
	Velocity += -FVector::UpVector * YanGrapple::GrapplingGravityMagnitude * DeltaSeconds;

	FVector PullDir = ToMeetPoint / Distance;

	// 站在地上时先掺入向上分量脱离地面，并把移动模式推向下落——
	// 行走模式会把水平速度约束在地面上，牵引因而拉不动人
	FFloorCheckResult FloorResult;
	if (SimBlackboard && SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult) && FloorResult.IsWalkableFloor())
	{
		PullDir                       = (PullDir + FVector::UpVector * YanGrapple::GroundLiftRatio).GetSafeNormal();
		OutProposedMove.PreferredMode = DefaultModeNames::Falling;
	}

	Velocity += PullDir * PullAcceleration * DeltaSeconds;

	const float Speed = Velocity.Size();
	if (Speed > MaxPullSpeed && Speed > 0.0f)
	{
		Velocity *= MaxPullSpeed / Speed;
	}

	OutProposedMove.MixMode        = MixMode;
	OutProposedMove.LinearVelocity = Velocity;

	return true;
}

FLayeredMoveBase* FYanLayeredMove_GrapplePull::Clone() const
{
	return new FYanLayeredMove_GrapplePull(*this);
}

void FYanLayeredMove_GrapplePull::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	SerializePackedVector<10, 24>(MeetPoint, Ar);

	Ar << PullAcceleration;
	Ar << MaxPullSpeed;
	Ar << StopDistance;
}

UScriptStruct* FYanLayeredMove_GrapplePull::GetScriptStruct() const
{
	return FYanLayeredMove_GrapplePull::StaticStruct();
}

FString FYanLayeredMove_GrapplePull::ToSimpleString() const
{
	return FString::Printf(TEXT("GrapplePull"));
}

void FYanLayeredMove_GrapplePull::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}
