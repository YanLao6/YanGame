#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/ChaosMovementModeTransition.h"
#include "MoverSimulationTypes.h"

#include "ChaosCharacterClinchCheck.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * UChaosCharacterClinchCheck - 贴身抓取的发起端入口，把角色送到落位点并锁死水平位移。
 *
 * 由输入包脉冲位 FYanCharacterInputs::bWantsToClinch 触发，落位点与下落基准时刻均直接
 * 取自输入包——落位点依赖被抓方的位置，而该位置在发起端是插值外推的结果，
 * 两端各自重算必然分歧，故与瞬移同一契约：只信任发起端上传的坐标。
 *
 * 仅服务发起方。被抓方的同名效果由服务器经 sim action 下发，不经过本 Transition，
 * 因而此处入队的效果既不接管朝向也不可挣脱。
 *
 * 挂载位置决定何时可发起；当前设计只在空中冲刺途中触发，挂在下落模式上即可。
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UChaosCharacterClinchCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UE_API UChaosCharacterClinchCheck(const FObjectInitializer& ObjectInitializer);

	/** 贴身期间所处的运动模式。取空中模式以避开地面摩擦对水平锁定的干扰，落地后由落地判定自行切走 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clinch")
	FName TransitionToMode = FName("Falling");

	/** 缓动段时长（毫秒），决定贴上去的快慢。须与被抓方下发的效果取同一值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clinch", meta = (ClampMin = "1", UIMin = "30", UIMax = "300", ForceUnits = "ms"))
	float PullDurationMs = 80.f;

	/** 缓动段的时间重映射指数：小于 1 起步快、末端收得缓 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clinch", meta = (ClampMin = "0.01", UIMin = "0.3", UIMax = "2"))
	float EaseExponent = 0.5f;

	/** 下落段水平回正的速度上限（cm/s） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clinch", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxHoldSpeed = 600.f;

	/** 贴身效果的存续上限（毫秒），到期强制解除以免异常状态下被永久锁住 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clinch", meta = (ClampMin = "100", ForceUnits = "ms"))
	float MaxDurationMs = 8000.f;

	//~Begin UBaseMovementModeTransition Interface
	/** 输入包请求贴身且落位点有效时切换到 TransitionToMode */
	UE_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;

	/** 以本帧位置为起点、输入包落位点为终点入队贴身效果 */
	UE_API virtual void Trigger_Implementation(const FSimulationTickParams& Params) override;
	//~End UBaseMovementModeTransition Interface
};

#undef UE_API
