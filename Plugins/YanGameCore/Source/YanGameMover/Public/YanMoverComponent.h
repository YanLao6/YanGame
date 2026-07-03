// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/Character/ChaosCharacterMoverComponent.h"
#include "YanMoverComponent.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 项目角色 Mover 组件。
 *
 * 继承 UChaosCharacterMoverComponent，模拟运行在 Chaos 物理线程（异步），
 * 由父类构造默认装配 Chaos 后端与 Walking/Falling/Flying 模式。
 * 本类仅在 Walking 模式补一个跳跃转换（引擎默认未装配 JumpCheck）。
 */
UCLASS(MinimalAPI, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UYanMoverComponent : public UChaosCharacterMoverComponent
{
	GENERATED_BODY()

public:
	UE_API UYanMoverComponent();
};

#undef UE_API
