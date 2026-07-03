#include "MoverMode/ChaosGrapplingMode.h"

#include "MoverMode/ChaosGrapplingExitCheck.h"
#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "ChaosMover/Utilities/ChaosMoverQueryUtils.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverDataModelTypes.h"
#include "YanMoverDataModelTypes.h"
#include "MoveLibrary/WaterMovementUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosGrapplingMode)

UChaosGrapplingMode::UChaosGrapplingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 在继承自 falling 的 LandingCheck 之外，追加钩锁失效时切回 Falling 的退出判定
	Transitions.Add(CreateDefaultSubobject<UChaosGrapplingExitCheck>(TEXT("GrapplingExitCheck")));
}

void UChaosGrapplingMode::GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	if (!Simulation)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("No Simulation set on ChaosGrapplingMode"));
		return;
	}

	const FChaosMoverSimulationDefaultInputs* DefaultSimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>();
	if (!DefaultSimInputs)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("ChaosGrapplingMode requires FChaosMoverSimulationDefaultInputs"));
		return;
	}

	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	// 锚点与激活状态从输入包读取，保证服务器 re-simulation 与客户端一致
	const FYanCharacterInputs* YanInputs = StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();

	const float   DeltaSeconds = TimeStep.StepMs * 0.001f;
	const FVector CurrentPos   = StartingSyncState->GetLocation_WorldSpace();
	FVector       Velocity     = StartingSyncState->GetVelocity_WorldSpace();

	// 重力：沿用 falling 约定，此处累加重力速度，SimulationTick 再扣除 PhysicsObjectGravity 物理补偿，避免重复计入
	Velocity += UMovementUtils::ComputeVelocityFromGravity(DefaultSimInputs->Gravity, DeltaSeconds);

	// 朝锚点的主动拉力
	if (YanInputs)
	{
		const FVector ToAnchor  = YanInputs->GrappleAnchorPoint - CurrentPos;
		const float   Dist      = ToAnchor.Size();
		const FVector AnchorDir = (Dist > 0.0001f) ? (ToAnchor / Dist) : DefaultSimInputs->UpDir;
		Velocity                += AnchorDir * PullAcceleration * DeltaSeconds;
	}

	// 对合速度限速
	const float Speed = Velocity.Size();
	if (Speed > MaxSwingSpeed && Speed > 0.f)
	{
		Velocity *= MaxSwingSpeed / Speed;
	}

	OutProposedMove.LinearVelocity = Velocity;
	OutProposedMove.MixMode        = EMoveMixMode::OverrideVelocity;
	// 钩锁不接受方向输入，维持当前朝向
	OutProposedMove.bHasDirIntent          = false;
	OutProposedMove.AngularVelocityDegrees = FVector::ZeroVector;

	// 更新地面查询结果，供继承自 falling 的 LandingCheck 在撞地时切回行走
	UpdateFloorSweep_Internal(OutProposedMove, DefaultSimInputs, DeltaSeconds, CurrentPos);
}

void UChaosGrapplingMode::UpdateFloorSweep_Internal(FProposedMove& OutProposedMove, const FChaosMoverSimulationDefaultInputs* DefaultSimInputs, const float DeltaSeconds, const FVector CurrentPos) const
{
	FFloorCheckResult FloorResult;
	FWaterCheckResult WaterResult;
	if (UMoverBlackboard* SimBlackboard = Simulation->GetBlackboard_Mutable())
	{
		UE::ChaosMover::Utils::FFloorSweepParams SweepParams{
			.ResponseParams     = DefaultSimInputs->CollisionResponseParams,
			.QueryParams        = DefaultSimInputs->CollisionQueryParams,
			.Location           = CurrentPos,
			.DeltaPos           = OutProposedMove.LinearVelocity * DeltaSeconds,
			.UpDir              = DefaultSimInputs->UpDir,
			.World              = DefaultSimInputs->World,
			.QueryDistance      = 1.2f * GetTargetHeight(),
			.QueryRadius        = FMath::Min(GetGroundQueryRadius(), FMath::Max(DefaultSimInputs->PawnCollisionRadius - 5.0f, 0.0f)),
			.MaxWalkSlopeCosine = GetMaxWalkSlopeCosine(),
			.TargetHeight       = GetTargetHeight(),
			.CollisionChannel   = DefaultSimInputs->CollisionChannel
		};

		UE::ChaosMover::Utils::FloorSweep_Internal(SweepParams, FloorResult, WaterResult);

		SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorResult);
		SimBlackboard->Set(CommonBlackboard::LastWaterResult, WaterResult);
	}
}
