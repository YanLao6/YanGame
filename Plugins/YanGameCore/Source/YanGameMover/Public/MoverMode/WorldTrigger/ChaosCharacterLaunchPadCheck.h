#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/ChaosMovementModeTransition.h"
#include "MoverSimulationTypes.h"

#include "ChaosCharacterLaunchPadCheck.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * UChaosCharacterLaunchPadCheck - 发射板：把输入包给出的速度叠加到角色上并切入空中模式。
 *
 * 由 FYanCharacterInputs::bWantsWorldLaunch 触发，速度直接取 WorldLaunchVelocity——
 * 它取决于发射板参数与 Pawn 朝向，物理线程访问不到 Actor，只信任发起端上传的值。
 * 效果（FYanWorldLaunchEffect）在 sim 内以可回滚方式入队，客户端预测与服务器重模拟得到同一结果；
 * 效果同时把请求序号记进 FYanWorldTriggerState，同一份输入包被多个物理步复用时不再重复触发。
 *
 * 切入 Falling 是必要的：行走模式的地面约束会把竖直速度吃掉，与引擎 UChaosCharacterJumpCheck 同理。
 * 挂在 MoverComponent 的全局 Transitions 上，任何模式下踩板都生效。
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UChaosCharacterLaunchPadCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UE_API UChaosCharacterLaunchPadCheck(const FObjectInitializer& ObjectInitializer);

	/** 弹射后切入的运动模式 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaunchPad")
	FName TransitionToMode = FName("Falling");

	//~Begin UBaseMovementModeTransition Interface
	UE_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	UE_API virtual void Trigger_Implementation(const FSimulationTickParams& Params) override;
	//~End UBaseMovementModeTransition Interface
};

#undef UE_API
