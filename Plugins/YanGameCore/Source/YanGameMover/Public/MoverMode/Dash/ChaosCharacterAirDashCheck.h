#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/ChaosMovementModeTransition.h"
#include "MoverSimulationTypes.h"

#include "ChaosCharacterAirDashCheck.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * UChaosCharacterAirDashCheck - 空中冲刺，沿玩家意图方向施加一次瞬时速度。
 *
 * 由输入包脉冲位 FYanCharacterInputs::bWantsToAirDash 触发，方向取本帧移动输入
 * （无输入时回退到视角前向），二者均来自输入命令包，客户端与服务器在重模拟时
 * 求得相同结果，冲刺不会被权威状态回滚修正。
 *
 * 「身处空中」由挂载位置保证：本 Transition 只应挂在空中模式（Falling 等）上。
 * 不查询 SimBlackboard 的地面结果——黑板不参与回滚，用它判定会引入新的不确定性。
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UChaosCharacterAirDashCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UE_API UChaosCharacterAirDashCheck(const FObjectInitializer& ObjectInitializer);

	/** 冲刺后切换到的运动模式，与挂载模式相同即为原地重入 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirDash")
	FName TransitionToMode = FName("Falling");

	/** 冲刺速度大小（cm/s），叠加到当前速度上 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirDash", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float DashMagnitude = 1500.f;

	//~Begin UBaseMovementModeTransition Interface
	/** 输入包请求空中冲刺时切换到 TransitionToMode */
	UE_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;

	/** 解算冲刺方向并施加瞬时速度 */
	UE_API virtual void Trigger_Implementation(const FSimulationTickParams& Params) override;
	//~End UBaseMovementModeTransition Interface
};

#undef UE_API
