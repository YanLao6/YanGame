#include "MoverMode/WallClimb/ChaosWallClimbEnterCheck.h"

#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "MoverDataModelTypes.h"
#include "MoverMode/WallClimb/WallClimbQueryUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosWallClimbEnterCheck)

UChaosWallClimbEnterCheck::UChaosWallClimbEnterCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;
}

FTransitionEvalResult UChaosWallClimbEnterCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	using namespace UE::YanMover::WallClimb;

	if (!Simulation)
	{
		return FTransitionEvalResult::NoTransition;
	}

	const FChaosMoverSimulationDefaultInputs* DefaultSimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>();
	const FCharacterDefaultInputs*            CharacterInputs  = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FMoverDefaultSyncState*             SyncState        = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!DefaultSimInputs || !CharacterInputs || !SyncState)
	{
		return FTransitionEvalResult::NoTransition;
	}

	const FVector UpDir = DefaultSimInputs->UpDir;

	// 上升段不吸附：起跳后须先转为下落才允许重新附着，否则贴墙起跳会在同一面墙上立即复位
	if (FVector::DotProduct(SyncState->GetVelocity_WorldSpace(), UpDir) > MinUpSpeed)
	{
		return FTransitionEvalResult::NoTransition;
	}

	// 无移动输入即无攀爬意图，先行排除，同时省去一次物理查询
	const FVector MoveInput = CharacterInputs->GetMoveInput_WorldSpace();
	if (MoveInput.IsNearlyZero())
	{
		return FTransitionEvalResult::NoTransition;
	}

	const FVector ProbeDir = FVector::VectorPlaneProject(CharacterInputs->ControlRotation.Vector(), UpDir).GetSafeNormal();
	if (ProbeDir.IsNearlyZero())
	{
		return FTransitionEvalResult::NoTransition;
	}

	const FWallSweepParams SweepParams{
		.ResponseParams      = DefaultSimInputs->CollisionResponseParams,
		.QueryParams         = DefaultSimInputs->CollisionQueryParams,
		.Location            = SyncState->GetLocation_WorldSpace(),
		.ProbeDir            = ProbeDir,
		.UpDir               = UpDir,
		.World               = DefaultSimInputs->World,
		.ProbeDistance       = MaxWallDistance,
		.ProbeRadius         = WallProbeRadius,
		.PawnCollisionRadius = DefaultSimInputs->PawnCollisionRadius,
		.CollisionChannel    = DefaultSimInputs->CollisionChannel
	};

	FWallCheckResult WallResult;
	if (!WallSweep_Internal(SweepParams, WallResult) || WallResult.Distance > MaxWallDistance)
	{
		return FTransitionEvalResult::NoTransition;
	}

	if (!IsClimbableAngle(WallResult.Normal, UpDir, MinWallAngle, MaxWallAngle))
	{
		return FTransitionEvalResult::NoTransition;
	}

	if (!IsFacingWall(CharacterInputs->ControlRotation.Vector(), WallResult.Normal, UpDir, ViewFacingTolerance))
	{
		return FTransitionEvalResult::NoTransition;
	}

	if (!IsFacingWall(MoveInput, WallResult.Normal, UpDir, MoveIntentTolerance))
	{
		return FTransitionEvalResult::NoTransition;
	}

	return FTransitionEvalResult(TransitionToMode);
}
