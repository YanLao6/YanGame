// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Abilities/GameplayAbilityTargetActor_Trace.h"

#include "GameplayAbilityTargetActor_Interact.generated.h"

class AActor;
class UObject;


/** 交互类目标 Actor 的中间基类，实现按视线单线检测的 PerformTrace。 */
UCLASS(MinimalAPI, Blueprintable)
class AGameplayAbilityTargetActor_Interact : public AGameplayAbilityTargetActor_Trace
{
	GENERATED_BODY()

public:
	AGameplayAbilityTargetActor_Interact(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin AGameplayAbilityTargetActor_Trace Interface
	virtual FHitResult PerformTrace(AActor* InSourceActor) override;
	//~End AGameplayAbilityTargetActor_Trace Interface

protected:
};
