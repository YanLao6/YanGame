#include "MoverMode/Attraction/ChaosAttractionMode.h"

#include "MoverDataModelTypes.h"
#include "MoverMode/Attraction/YanAttractionState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosAttractionMode)

FVector UChaosAttractionMode::ComputeMoveInput(const FYanAttractionSource& Source, const FVector& PawnLocation, double SimTimeMs, float FlyingMaxSpeed) const
{
	const float SpeedLimit = FMath::Min(FMath::Max(Source.Settings.PullSpeed, 0.0f), FMath::Max(FlyingMaxSpeed, 0.0f));
	if (SpeedLimit <= UE_KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}
	const FVector ToCenter = Source.Settings.GetCenter(SimTimeMs) - PawnLocation;
	const float Distance = ToCenter.Size();
	const float ArrivalScale = FMath::Clamp((Distance - FMath::Max(StopDistance, 0.0f)) / FMath::Max(SlowdownDistance, 1.0f), 0.0f, 1.0f);
	// 快照超过租期后停止沿旧速度飞行，锁定关系仍由来源显式释放。
	const bool bHasFreshVelocity = SimTimeMs < Source.Settings.SnapshotSimTimeMs + Source.Settings.DurationMs;
	const FVector CenterVelocity = bHasFreshVelocity ? Source.Settings.CenterVelocity : FVector::ZeroVector;
	const FVector DesiredVelocity = (CenterVelocity + ToCenter.GetSafeNormal() * SpeedLimit * ArrivalScale).GetClampedToMaxSize(SpeedLimit);
	return DesiredVelocity / FlyingMaxSpeed;
}

void UChaosAttractionMode::GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	const FMoverDefaultSyncState* Sync = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const FYanAttractionState* State = StartState.SyncState.SyncStateCollection.FindDataByType<FYanAttractionState>();
	const FYanAttractionSource* Source = Sync && State ? State->SelectSource(Sync->GetLocation_WorldSpace(), TimeStep.BaseSimTimeMs) : nullptr;

	// 仅替换本次飞行计算的输入视图，原客户端输入包保留供回滚和其他模式使用。
	FMoverTickStartData FlightStart;
	FlightStart = StartState;
	FCharacterDefaultInputs& FlightInputs = FlightStart.InputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	FlightInputs = FCharacterDefaultInputs();
	const FVector MoveInput = Source ? ComputeMoveInput(*Source, Sync->GetLocation_WorldSpace(), TimeStep.BaseSimTimeMs, GetMaxSpeed()) : FVector::ZeroVector;
	FlightInputs.SetMoveInput(EMoveInputType::DirectionalIntent, MoveInput);
	Super::GenerateMove_Implementation(SimContext, FlightStart, TimeStep, OutProposedMove);
}

void UChaosAttractionMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const FMoverDefaultSyncState* Sync = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	FYanAttractionState* State = OutputState.SyncState.SyncStateCollection.FindMutableDataByType<FYanAttractionState>();
	if (Sync && State)
	{
		State->UpdateCapture(Sync->GetLocation_WorldSpace(), Params.TimeStep.BaseSimTimeMs);
	}
	if (!Sync || !State || !State->SelectSource(Sync->GetLocation_WorldSpace(), Params.TimeStep.BaseSimTimeMs))
	{
		OutputState.MovementEndState.NextModeName = ExitMovementMode;
		OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs;
		return;
	}
	Super::SimulationTick_Implementation(Params, OutputState);
}

UChaosAttractionEnterCheck::UChaosAttractionEnterCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;
}

FTransitionEvalResult UChaosAttractionEnterCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	if (TransitionToMode.IsNone() || Params.StartState.SyncState.MovementMode == TransitionToMode)
	{
		return FTransitionEvalResult::NoTransition;
	}
	const FMoverDefaultSyncState* Sync = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const FYanAttractionState* State = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FYanAttractionState>();
	return Sync && State && State->SelectSource(Sync->GetLocation_WorldSpace(), Params.TimeStep.BaseSimTimeMs)
		? FTransitionEvalResult(TransitionToMode) : FTransitionEvalResult::NoTransition;
}
