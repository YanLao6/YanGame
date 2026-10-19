#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "MoverSimulationTypes.h"
#include "ChaosMover/ChaosMovementModeTransition.h"

#include "ChaosGrapplingEnterCheck.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * UChaosGrapplingEnterCheck - 钩锁运动模式的进入判定。
 *
 * 挂在可进入钩锁的模式（Walking / Falling）上：当输入包
 * FYanCharacterInputs::bIsGrapplingActive 为 true（由 MoverGrapplingAbility 命中后写入）时，
 * 且钩子处于视角水平前向左右各 90° 范围内时，切入 TransitionToMode。
 *
 * 与 UChaosGrapplingExitCheck 对称：进入/退出均在 sim 内依据同一持续标志裁决，
 * 服务器重放与客户端预测一致，取代了不可复现的一次性 SuggestedMovementMode 进入路径。
 */
UCLASS(MinimalAPI, Blueprintable, EditInlineNew, DefaultToInstanced)
class UChaosGrapplingEnterCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UE_API UChaosGrapplingEnterCheck(const FObjectInitializer& ObjectInitializer);

	/** 钩锁激活时切入的运动模式（须与 UChaosGrapplingMode 在 MoverComponent 上的注册名一致） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Mover|Grappling")
	FName TransitionToMode = FName("Grappling");

	//~Begin UBaseMovementModeTransition Interface
	/** 牵引意图有效且钩子在有效方向范围内时切换到 TransitionToMode。 */
	UE_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	//~End UBaseMovementModeTransition Interface
};

#undef UE_API
