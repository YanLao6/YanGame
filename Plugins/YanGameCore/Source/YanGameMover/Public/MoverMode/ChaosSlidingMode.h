#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/Character/Modes/ChaosCharacterMovementMode.h"

#include "ChaosSlidingMode.generated.h"

struct FFloorCheckResult;
struct FWaterCheckResult;

/**
 * UChaosSlidingMode - Chaos 版滑行运动模式，与 UChaosWalkingMode 平级，继承 UChaosCharacterMovementMode。
 *
 * 设计意图（对应旧 USlidingMode 的 Chaos 化）：
 *  - GenerateMove：不接受方向输入（bHasDirIntent=false），对水平速度做摩擦衰减，并叠加坡度重力，
 *                 使角色顺坡滑行。决策结果写入 ProposedMove，作为本帧的运动目标。
 *  - SimulationTick：参照 UChaosWalkingMode，根据地面查询结果把角色贴合到目标高度并沿地面推进，
 *                   实际位移由 Character Ground Constraint 施力逼近 target，而非直接 sweep。
 *  - UpdateConstraintSettings：设置贴地约束的摩擦力上限与阻尼等参数。
 *
 * 说明：为避免引入对 Chaos 模块内部接口的直接依赖，本模式未重写 ModifyContacts（沿用基类默认），
 *      且 SimulationTick 未补偿动态地面自身的重力下落分量。对平地、斜坡、匀速移动平台上的滑行无影响。
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class YANGAMEMOVER_API UChaosSlidingMode : public UChaosCharacterMovementMode
{
	GENERATED_BODY()

public:
	UChaosSlidingMode(const FObjectInitializer& ObjectInitializer);

	/** 地面摩擦系数（越小越滑），每帧按 dV = SlideFriction * |V| * dt 对水平速度衰减 */
	UPROPERTY(EditDefaultsOnly, Category = "Slide", meta = (ClampMin = "0", UIMin = "0"))
	float SlideFriction = 0.1f;

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
	virtual void UpdateConstraintSettings(Chaos::FCharacterGroundConstraintSettings& ConstraintSettings) const override;
	//~End UChaosCharacterMovementMode Interface

	//~Begin UBaseMovementMode Interface
	/** 计算摩擦衰减 + 坡度重力的速度；屏蔽方向输入，不接受加速度驱动 */
	virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

	/** 依据地面结果计算 target position/velocity 并写入输出同步状态，由地面约束驱动贴地推进 */
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
	//~End UBaseMovementMode Interface

protected:
	/** 判断命中面是否可作为台阶踏上：台阶高度在 MaxStepHeight 内且表面允许踏上 */
	bool CanStepUpOnHitSurface(const FFloorCheckResult& FloorResult) const;

	/** 在提议移动的落点做地面扫描，更新可行走地面结果，并约束移动避免冲入不可行走面 */
	void GetFloorAndCheckMovement(const FMoverDefaultSyncState& SyncState, const FProposedMove& ProposedMove, const FChaosMoverSimulationDefaultInputs& DefaultSimInputs, float DeltaSeconds, FFloorCheckResult& OutFloorResult, FWaterCheckResult& OutWaterResult, FVector& OutDeltaPos) const;
};
