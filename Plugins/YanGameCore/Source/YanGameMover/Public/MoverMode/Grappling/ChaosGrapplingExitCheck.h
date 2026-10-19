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
 * 挂在 UChaosGrapplingMode 上：牵引意图失效，或钩子到达玩家视角水平前向的分界面及后方时退出。
 * 方向只从仿真同步状态与输入包读取，供进入判定和技能清理复用。
 */
UCLASS(MinimalAPI, Blueprintable, EditInlineNew, DefaultToInstanced)
class UChaosGrapplingExitCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UE_API UChaosGrapplingExitCheck(const FObjectInitializer& ObjectInitializer);

	/** 钩锁失效后切换到的运动模式 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Mover|Grappling")
	FName TransitionToMode = FName("Falling");

	/** 分界面保持世界 Z 轴竖直，只跟随视角 Yaw；钩子在面上、面后或缺少判定数据时返回 true。 */
	UFUNCTION(BlueprintPure, Category = "Yan|Mover|Grappling")
	static UE_API bool IsHookOutsideForwardHemisphere(const FMoverSyncState& SyncState, const FMoverInputCmdContext& InputCmd);

	//~Begin UBaseMovementModeTransition Interface
	/** 牵引意图失效或钩子不在有效方向范围内时切换到 TransitionToMode。 */
	UE_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	//~End UBaseMovementModeTransition Interface
};

#undef UE_API
