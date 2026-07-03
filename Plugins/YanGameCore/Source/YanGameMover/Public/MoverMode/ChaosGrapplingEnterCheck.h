#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "MoverSimulationTypes.h"
#include "ChaosMover/ChaosMovementModeTransition.h"

#include "ChaosGrapplingEnterCheck.generated.h"

/**
 * UChaosGrapplingEnterCheck - 钩锁运动模式的进入判定。
 *
 * 挂在可进入钩锁的模式（Walking / Falling）上：当输入包
 * FYanCharacterInputs::bIsGrapplingActive 为 true（由 MoverGrapplingAbility 命中后写入）时，
 * 切入 TransitionToMode。
 *
 * 与 UChaosGrapplingExitCheck 对称：进入/退出均在 sim 内依据同一持续标志裁决，
 * 服务器重放与客户端预测一致，取代了不可复现的一次性 SuggestedMovementMode 进入路径。
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced)
class YANGAMEMOVER_API UChaosGrapplingEnterCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UChaosGrapplingEnterCheck(const FObjectInitializer& ObjectInitializer);

	/** 钩锁激活时切入的运动模式（须与 UChaosGrapplingMode 在 MoverComponent 上的注册名一致） */
	UPROPERTY(EditDefaultsOnly, Category = "Grappling")
	FName TransitionToMode = FName("Grappling");

	//~Begin UBaseMovementModeTransition Interface
	/** bIsGrapplingActive 为 true 时返回切换到 TransitionToMode，否则不切换 */
	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	//~End UBaseMovementModeTransition Interface
};
