#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "MoverSimulationTypes.h"
#include "ChaosMover/ChaosMovementModeTransition.h"

#include "ChaosGrapplingExitCheck.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * UChaosGrapplingExitCheck - 钩锁运动模式的退出判定。
 *
 * 挂在 UChaosGrapplingMode 上：当输入包 FYanCharacterInputs::bIsGrapplingActive 转为 false
 * （松手 / 超时 / 锚点进入后半球，均由 MoverGrapplingAbility 写入）时，切回 TransitionToMode。
 */
UCLASS(MinimalAPI, Blueprintable, EditInlineNew, DefaultToInstanced)
class UChaosGrapplingExitCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UE_API UChaosGrapplingExitCheck(const FObjectInitializer& ObjectInitializer);

	/** 钩锁失效后切换到的运动模式 */
	UPROPERTY(EditDefaultsOnly, Category = "Grappling")
	FName TransitionToMode = FName("Falling");

	//~Begin UBaseMovementModeTransition Interface
	/** bIsGrapplingActive 为 false 时返回切换到 TransitionToMode，否则不切换 */
	UE_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	//~End UBaseMovementModeTransition Interface
};

#undef UE_API
