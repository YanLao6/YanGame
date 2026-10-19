#include "MoverLibrary/MoverSimulationLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverSimulationLibrary)

void UMoverSimulationLibrary::InitTickEndForNewFrame(FMoverTickEndData& TickEndData)
{
	TickEndData.InitForNewFrame();
}

void UMoverSimulationLibrary::ResetMovementModeTickEndState(FMovementModeTickEndState& EndState)
{
	EndState.ResetToDefaults();
}

void UMoverSimulationLibrary::ResetMovementRecord(FMovementRecord& Record)
{
	Record.Reset();
}

FMovementSubstep UMoverSimulationLibrary::MakeMovementSubstep(FName MoveName, FVector MoveDelta, bool bIsRelevant)
{
	return FMovementSubstep(MoveName, MoveDelta, bIsRelevant);
}

void UMoverSimulationLibrary::AppendMovementSubstep(FMovementRecord& Record, FMovementSubstep Substep)
{
	Record.Append(Substep);
}

TArray<FMovementSubstep> UMoverSimulationLibrary::GetMovementRecordSubsteps(const FMovementRecord& Record)
{
	return TArray<FMovementSubstep>(Record.GetSubsteps());
}

void UMoverSimulationLibrary::LockMovementRecordRelevancy(FMovementRecord& Record, bool bLockValue)
{
	Record.LockRelevancy(bLockValue);
}

void UMoverSimulationLibrary::UnlockMovementRecordRelevancy(FMovementRecord& Record)
{
	Record.UnlockRelevancy();
}

FString UMoverSimulationLibrary::MovementRecordToString(const FMovementRecord& Record)
{
	return Record.ToString();
}
