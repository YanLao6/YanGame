// Fill out your copyright notice in the Description page of Project Settings.


#include "YanMovementMode.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(YanMovementMode)


void UYanMovementMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	Super::SimulationTick_Implementation(Params, OutputState);

	K2_OnSimulationTick(Params, OutputState);
}

void UYanMovementMode::GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	Super::GenerateMove_Implementation(SimContext, StartState, TimeStep, OutProposedMove);

	K2_OnGenerateMove(StartState, TimeStep, OutProposedMove);
}
