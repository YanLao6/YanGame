#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/ChaosMovementModeTransition.h"
#include "MoverSimulationTypes.h"

#include "ChaosWallClimbExitCheck.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * UChaosWallClimbExitCheck - 爬墙的退出判定，挂在 UChaosWallClimbMode 上。
 *
 * 三种情况切回下落：
 *  - 玩家主动脱墙（FYanCharacterInputs::bWantsToDetachFromWall，由下蹲键写入）
 *  - 墙面丢失，依据爬墙模式上一帧记录于 FYanWallClimbState 的法线判定
 *  - 爬墙耗尽，依据 FYanWallClimbState 的累计时长与所属 UChaosWallClimbMode 的 MaxClimbDurationMs 判定
 *
 * 视角或移动意图背离墙面不在此列——那只影响能否主动移动，不影响附着。
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UChaosWallClimbExitCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UE_API UChaosWallClimbExitCheck(const FObjectInitializer& ObjectInitializer);

	/** 脱离墙面后切换到的运动模式 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|WallClimb")
	FName TransitionToMode = FName("Falling");

	/** 脱墙时沿 UpDir 叠加的瞬时速度（cm/s），设为零可禁用 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|WallClimb", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float ExitUpwardsSpeed = 300.f;

	//~Begin UBaseMovementModeTransition Interface
	/** 主动脱墙、墙面丢失或爬墙耗尽时切换到 TransitionToMode */
	UE_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;

	/** 退出时一次性叠加向上速度 */
	UE_API virtual void Trigger_Implementation(const FSimulationTickParams& Params) override;
	//~End UBaseMovementModeTransition Interface
};

#undef UE_API
