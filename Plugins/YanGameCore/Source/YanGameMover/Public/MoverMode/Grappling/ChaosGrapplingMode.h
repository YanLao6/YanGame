#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/Character/Modes/ChaosFallingMode.h"

#include "ChaosGrapplingMode.generated.h"

#define UE_API YANGAMEMOVER_API

namespace YanGrapple
{
	/**
	 * 钩锁牵引期间的重力大小（cm/s^2），沿重力方向作用。
	 *
	 * 直接给定数值而非缩放世界重力：牵引手感由拉力与这一固定值共同决定，
	 * 若随环境重力浮动，同一套拉力参数在不同重力设置下会给出完全不同的弧线。
	 * 取值为 UE 默认重力 980 的一半。
	 *
	 * 发起者侧由 UChaosGrapplingMode 使用，被钩者侧由 FYanLayeredMove_GrapplePull 使用，
	 * 两侧共用同一常量，牵引手感才对称。
	 */
	inline constexpr float GrapplingGravityMagnitude = 490.0f;

	/**
	 * 站在可行走地面上时掺入拉力方向的向上分量比例。
	 *
	 * 近乎水平的拉力拉不动站在地上的人：钩锁牵引派生自下落，落地判定会在当帧
	 * 把角色判回行走模式，两个模式来回切换，牵引因而被吃掉大半。
	 * 先给一个向上分量脱离地面，此后按空中情形正常牵引。
	 */
	inline constexpr float GroundLiftRatio = 0.4f;
}

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
 *  - GenerateMove 屏蔽方向输入，改为：钩锁重力（YanGrapple::GrapplingGravityMagnitude，
 *    沿用 falling 约定由 SimulationTick 做物理补偿）+ 朝输入包汇合点
 *    FYanCharacterInputs::GrappleAnchorPoint 的主动拉力 + 合速度限速。
 *  - 进入由 UChaosGrapplingEnterCheck 判定；退出由构造函数挂入的 UChaosGrapplingExitCheck
 *    依据牵引意图与视角水平前向的有效方向范围判定。
 *
 * 汇合点可能是墙面命中点，也可能是与被钩 Pawn 之间的会合位置——本模式不区分两者，
 * 「往哪里拉」一律由发起端算定并经输入包给出。被钩者那一侧的对称牵引由
 * FYanLayeredMove_GrapplePull 承载，两者公式相同。
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
	// 上一帧扫描所得的地面是否可行走，用于判断拉力是否需要先掺入离地分量
	UE_API bool IsOnWalkableFloor_Internal() const;

	UE_API void UpdateFloorSweep_Internal(FProposedMove& OutProposedMove, const FChaosMoverSimulationDefaultInputs* DefaultSimInputs, float DeltaSeconds, FVector CurrentPos) const;
};

#undef UE_API
