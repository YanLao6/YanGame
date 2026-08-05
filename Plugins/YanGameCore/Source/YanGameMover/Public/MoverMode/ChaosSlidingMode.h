#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/Character/Modes/ChaosCharacterMovementMode.h"

#include "ChaosSlidingMode.generated.h"

#define UE_API YANGAMEMOVER_API

struct FFloorCheckResult;
struct FWaterCheckResult;

/**
 * UChaosSlidingMode - Chaos 版滑行运动模式，与 UChaosWalkingMode 平级，继承 UChaosCharacterMovementMode。
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UChaosSlidingMode : public UChaosCharacterMovementMode
{
	GENERATED_BODY()

public:
	UE_API UChaosSlidingMode(const FObjectInitializer& ObjectInitializer);

	/** 地面摩擦系数（越小越滑），每帧按 dV = SlideFriction * |V| * dt 对水平速度衰减 */
	UPROPERTY(EditDefaultsOnly, Category = "Slide", meta = (ClampMin = "0", UIMin = "0"))
	float SlideFriction = 0.1f;

	/** 正对下坡方向滑行时的摩擦系数（越小越滑），随移动方向与坡向夹角插值 */
	UPROPERTY(EditDefaultsOnly, Category = "Slide", meta = (ClampMin = "0", UIMin = "0"))
	float DownhillFriction = 0.4f;

	/** 正对上坡方向滑行时的摩擦系数（越大越难上坡），随移动方向与坡向夹角插值 */
	UPROPERTY(EditDefaultsOnly, Category = "Slide", meta = (ClampMin = "0", UIMin = "0"))
	float UphillFriction = 3.0f;

	/** 地面法线与上方向的夹角超过该值（度）即视为斜面，改用上/下坡插值摩擦 */
	UPROPERTY(EditDefaultsOnly, Category = "Slide", meta = (ClampMin = "0", UIMin = "0", ClampMax = "90", UIMax = "90", ForceUnits = "degrees"))
	float SlopeAngleThreshold = 5.0f;

	/** 水平速度（cm/s）低于该阈值视为滑行结束，供退出 Transition 判定使用 */
	UPROPERTY(EditDefaultsOnly, Category = "Slide", meta = (ClampMin = "0", UIMin = "0"))
	float SlideEndSpeed = 100.0f;

	/** 角色与地面交互的阻尼（0 为无阻尼，1 为最大阻尼） */
	UPROPERTY(EditAnywhere, Category = "Constraint Settings", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float GroundDamping = 0.0f;

	/** 站在不可行走斜坡上时，角色为保持原地所能施加的最大摩擦力 */
	UPROPERTY(EditAnywhere, Category = "Constraint Settings", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "Newtons"))
	float FrictionForceLimit = 100.0f;

	/** 径向力上限的缩放，使角色总能达到 motion target（1 按需放大，0 不缩放） */
	UPROPERTY(EditAnywhere, Category = "Constraint Settings", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float FractionalRadialForceLimitScaling = 1.0f;

	/** 角色移动时施加到地面平面内的反作用力比例（1 为全反力，0 为仅法向力） */
	UPROPERTY(EditAnywhere, Category = "Constraint Settings", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float FractionalGroundReaction = 1.0f;

	/** 角色处于 MaxStepHeight 范围内时，用于贴地的向下速度比例 */
	UPROPERTY(EditAnywhere, Category = "Movement Settings", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float FractionalDownwardVelocityToTarget = 1.0f;

	//~Begin UChaosCharacterMovementMode Interface
	/** 将贴地约束的摩擦力上限、阻尼、反作用力等参数写入求解器设置 */
	UE_API virtual void UpdateConstraintSettings(Chaos::FCharacterGroundConstraintSettings& ConstraintSettings) const override;
	//~End UChaosCharacterMovementMode Interface

	//~Begin UBaseMovementMode Interface
	/** 计算摩擦衰减 + 坡度重力的速度；屏蔽方向输入，不接受加速度驱动 */
	UE_API virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

	/** 依据地面结果计算 target position/velocity 并写入输出同步状态，由地面约束驱动贴地推进 */
	UE_API virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
	//~End UBaseMovementMode Interface

protected:
	/** 判断命中面是否可作为台阶踏上：台阶高度在 MaxStepHeight 内且表面允许踏上 */
	UE_API bool CanStepUpOnHitSurface(const FFloorCheckResult& FloorResult) const;

	/** 在提议移动的落点做地面扫描，更新可行走地面结果，并约束移动避免冲入不可行走面 */
	UE_API void GetFloorAndCheckMovement(const FMoverDefaultSyncState& SyncState, const FProposedMove& ProposedMove, const FChaosMoverSimulationDefaultInputs& DefaultSimInputs, float DeltaSeconds, FFloorCheckResult& OutFloorResult, FWaterCheckResult& OutWaterResult, FVector& OutDeltaPos) const;
};

#undef UE_API
