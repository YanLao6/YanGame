#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/ChaosMovementModeTransition.h"
#include "MoverSimulationTypes.h"

#include "ChaosCharacterTeleporterCheck.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * UChaosCharacterTeleporterCheck - 传送门：把角色传送到输入包给出的位姿，并可选覆盖出口速度。
 *
 * 由 FYanCharacterInputs::bWantsWorldTeleport 触发，位姿与出口速度取自输入包——
 * 传送门是静态 Actor，两端算出的目标一致，服务器只需按包执行。
 * 效果为 FYanWorldTeleportEffect，在 sim 内以可回滚方式入队；目标点被占据时由 CanTeleport 拒绝，
 * 角色原地不动、出口速度也不施加。效果同时把请求序号记进 FYanWorldTriggerState，
 * 同一份输入包被多个物理步复用时不再重复触发。
 *
 * 挂在 MoverComponent 的全局 Transitions 上，任何模式下踩门都生效。
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UChaosCharacterTeleporterCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UE_API UChaosCharacterTeleporterCheck(const FObjectInitializer& ObjectInitializer);

	/** 传送时切入的运动模式；出口通常有推出速度，切入空中模式免得地面约束把它吃掉 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleporter")
	FName TransitionToMode = FName("Falling");

	//~Begin UBaseMovementModeTransition Interface
	UE_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	UE_API virtual void Trigger_Implementation(const FSimulationTickParams& Params) override;
	//~End UBaseMovementModeTransition Interface
};

#undef UE_API
