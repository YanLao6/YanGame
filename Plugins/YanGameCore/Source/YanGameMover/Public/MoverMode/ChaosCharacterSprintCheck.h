#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "MoverSimulationTypes.h"
#include "ChaosMover/ChaosMovementModeTransition.h"
#include "ChaosCharacterSprintCheck.generated.h"

/**
 * 疾跑的进入/退出裁决。
 *
 * 疾跑是持续状态：本 Transition 只做判定，速度覆盖由 FYanSprintModifier 承载。
 *
 * 手感为「脉冲进入、条件维持」的锁定式：按键只提供一次进入脉冲，之后疾跑靠
 * 移动意愿本身维持，松开疾跑键不退出。故进入与退出的判据刻意不对称——
 * 进入须同时满足脉冲与维持条件，退出只看维持条件。
 *
 * 判定所需数据全部取自 InputCmd（FYanCharacterInputs 的按键脉冲位与
 * FCharacterDefaultInputs 的移动输入、视角朝向），不读取任何 game-thread 状态。
 * 服务器重模拟时用相同的 InputCmd 得到相同结论，不产生状态回滚抖动。
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced)
class YANGAMEMOVER_API UChaosCharacterSprintCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UChaosCharacterSprintCheck(const FObjectInitializer& ObjectInitializer);

	/** 疾跑期间的最大水平速度（cm/s），覆盖当前移动模式的原值 */
	UPROPERTY(EditDefaultsOnly, Category = "Sprint", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float SprintMaxSpeed = 900.0f;

	/** 疾跑期间的加速度（cm/s^2），覆盖当前移动模式的原值 */
	UPROPERTY(EditDefaultsOnly, Category = "Sprint", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s^2"))
	float SprintAcceleration = 4000.0f;

	/**
	 * 维持疾跑所需的「移动意愿·视线朝向」水平点积下限。
	 * 0 对应 90 度：一旦前进意愿与视线在平面上的夹角达到或超过 90 度即退出疾跑。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Sprint", meta = (ClampMin = "-1", UIMin = "-1", ClampMax = "1", UIMax = "1"))
	float MinFacingDot = 0.0f;

	//~Begin UBaseMovementModeTransition Interface
	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	virtual void                  Trigger_Implementation(const FSimulationTickParams& Params) override;
	//~End UBaseMovementModeTransition Interface

protected:
	/** 进入脉冲：疾跑键在本帧刚按下 */
	bool WantsToStartSprint(const FSimulationTickParams& Params) const;

	/**
	 * 疾跑维持条件：脚踩可行走地面、未处于蹲伏、且移动意愿未背离视线。
	 * 刻意不含按键状态——按键只负责进入，维持与否与手是否还按着无关。
	 */
	bool CanSustainSprint(const FSimulationTickParams& Params) const;

	// Evaluate 为 const，判定结果经此传递给同帧的 Trigger
	mutable bool bTriggerSprint     = false;
	mutable bool bTriggerStopSprint = false;
};
