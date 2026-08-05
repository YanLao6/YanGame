#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/Character/Modes/ChaosFallingMode.h"

#include "ChaosGrapplingMode.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * UChaosGrapplingMode - 钩锁牵引的 ChaosMover 运动模式，继承 UChaosFallingMode。
 *
 * 范式背景：
 *  钩锁以独立 Movement Mode 实现，由 ChaosMover 在物理线程驱动 GenerateMove / SimulationTick。
 *  ChaosMover 亦支持 layered move（QueueLayeredMove / QueueLayeredMoveInstance），但仅限
 *  SupportsAsync() 为 true 的实现；该标志是纯 C++ virtual，AngelScript 侧无法重写。
 *
 * 设计要点：
 *  - 继承 UChaosFallingMode，复用其空中积分 SimulationTick 与 PhysicsObjectGravity 重力补偿，
 *    以及 LandingCheck（撞地时切回行走）。
 *  - GenerateMove 屏蔽方向输入，改为：重力（沿用 falling 约定，由 SimulationTick 做物理补偿）
 *    + 朝输入包锚点 FYanCharacterInputs::GrappleAnchorPoint 的主动拉力 + 合速度限速。
 *  - 进入由 MoverGrapplingAbility 通过 SuggestedMovementMode 触发；退出由构造函数挂入的
 *    UChaosGrapplingExitCheck 依据 bIsGrapplingActive 判定。
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UChaosGrapplingMode : public UChaosFallingMode
{
	GENERATED_BODY()

public:
	UE_API UChaosGrapplingMode(const FObjectInitializer& ObjectInitializer);

	/** 向锚点方向施加的主动拉力加速度（cm/s^2） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grappling", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s^2"))
	float PullAcceleration = 3000.f;

	/** 合速度上限（cm/s），对含重力与拉力的总速度做限制 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grappling", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float MaxSwingSpeed = 2500.f;

	//~Begin UBaseMovementMode Interface
	/** 重力 + 朝锚点拉力 + 合速度限速；屏蔽方向输入，覆盖式写入提议移动 */
	UE_API virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
	//~End UBaseMovementMode Interface
	
protected:
	UE_API void UpdateFloorSweep_Internal(FProposedMove& OutProposedMove, const FChaosMoverSimulationDefaultInputs* DefaultSimInputs, float DeltaSeconds, FVector CurrentPos) const;
};

#undef UE_API
