#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/Character/Modes/ChaosFallingMode.h"
#include "NativeGameplayTags.h"

#include "ChaosWallClimbMode.generated.h"

#define UE_API YANGAMEMOVER_API

struct FChaosMoverSimulationDefaultInputs;

/**
 * 附墙状态标签，由本模式在存续期间对外暴露。
 * 击退计算据此把人从墙上打下来——附墙时被击中不该还能挂在原处。
 */
YANGAMEMOVER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mover_IsWallClimbing);

/**
 * UChaosWallClimbMode - 攀爬墙面的 ChaosMover 运动模式，继承 UChaosFallingMode。
 *
 * 设计要点：
 *  - 继承 UChaosFallingMode 以复用其空中积分 SimulationTick（内含 PhysicsObjectGravity 补偿）
 *    与 LandingCheck。父类积分会扣除物理引擎即将施加的重力，故本模式在 GenerateMove 中
 *    不累加重力时净效果即为悬停。
 *  - 速度每帧投影到墙面切平面，进入时的动量因此自然继承，无需对首帧特殊处理。
 *  - 视角朝向墙面与否只决定能否主动加速，不决定去留：视角转离墙面时保留既有惯性、
 *    屏蔽新的输入意图，角色仍附着于墙。
 *  - 超过 MaxClimbDurationMs 即耗尽，下一步由 UChaosWallClimbExitCheck 据累计时长切回下落；
 *    再次进入的冷却由 UChaosWallClimbEnterCheck 负责，本模式只记录最后一次爬墙的帧号。
 *  - 退出由 UChaosWallClimbExitCheck（墙面丢失/主动脱墙/耗尽）、UChaosWallClimbJumpCheck（起跳）
 *    与继承自 falling 的 LandingCheck（落地）三方负责。
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UChaosWallClimbMode : public UChaosFallingMode
{
	GENERATED_BODY()

public:
	UE_API UChaosWallClimbMode(const FObjectInitializer& ObjectInitializer);

	/** 可攀爬墙面的最小倾角（度），即墙面法线与上方向的夹角下限 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Surface", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MinWallAngle = 80.f;

	/** 可攀爬墙面的最大倾角（度）。大于 90 度允许轻微外倾的墙面 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Surface", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MaxWallAngle = 100.f;

	/** 自胶囊表面向外的探墙距离（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Surface", meta = (ClampMin = "0", ForceUnits = "cm"))
	float WallProbeDistance = 60.f;

	/** 探墙球体扫描的半径（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Surface", meta = (ClampMin = "0", ForceUnits = "cm"))
	float WallProbeRadius = 20.f;

	/** 视角与墙面内法线的最大夹角（度），超出后失去主动移动能力但仍附着 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Control", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float ViewFacingTolerance = 45.f;

	/** 沿墙面移动的加速度（cm/s^2） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Control", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float ClimbAcceleration = 2400.f;

	/** 沿墙面移动的速度上限（cm/s） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Control", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxClimbSpeed = 400.f;

	/** 爬墙摩擦系数，按 dV = Friction * |V| * dt 衰减切向速度，决定继承动量的耗散快慢 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Control", meta = (ClampMin = "0"))
	float ClimbFriction = 3.f;

	/** 朝墙面的吸附速度（cm/s），维持附着并抵消轻微的几何起伏 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Control", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float WallStickSpeed = 100.f;

	/** 转向墙面的角速度上限（度/秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Control", meta = (ClampMin = "0", ForceUnits = "deg/s"))
	float TurnToWallRate = 540.f;

	/** 单次爬墙的最大时长（ms），超过后脱墙进入下落；0 表示不限时 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Stamina", meta = (ClampMin = "0", ForceUnits = "ms"))
	float MaxClimbDurationMs = 3000.f;

	//~Begin UBaseMovementMode Interface
	/** 沿墙面求解速度；墙面丢失时退化为父类的下落行为，由退出判定负责切走 */
	UE_API virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

	/** 复用父类空中积分，并在其后更新爬墙计时与墙面法线 */
	UE_API virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
	//~End UBaseMovementMode Interface

protected:
	/** 更新地面查询结果，供继承自 falling 的 LandingCheck 在触地时切回行走 */
	UE_API void UpdateFloorSweep_Internal(const FProposedMove& ProposedMove, const FChaosMoverSimulationDefaultInputs* DefaultSimInputs, float DeltaSeconds, const FVector& CurrentPos) const;
};

#undef UE_API
