// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Modes/SmoothWalkingMode.h"
#include "YanMovementMode.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 
 */
UCLASS(MinimalAPI)
class UYanMovementMode : public UWalkingMode
{
	GENERATED_BODY()

public:
	UE_API virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
	UE_API virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

	UFUNCTION(BlueprintImplementableEvent)
	void K2_OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState);

	UFUNCTION(BlueprintImplementableEvent)
	void K2_OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const;
};

#undef UE_API
