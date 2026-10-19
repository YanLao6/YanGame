#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/ChaosMovementModeTransition.h"
#include "MoverSimulationTypes.h"

#include "ChaosCharacterBlinkCheck.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * UChaosCharacterBlinkCheck - 瞬移，在固定时长内把角色送到输入包给定的终点。
 *
 * 由输入包脉冲位 FYanCharacterInputs::bWantsToBlink 触发，终点直接取自
 * FYanCharacterInputs::BlinkTargetPoint——终点须经碰撞扫描收敛，两端各自扫描不保证
 * 得到同一结果，故只信任发起端上传的坐标。起点则取本帧 SyncState 位置，
 * 该状态在客户端预测与服务器重模拟中一致。
 *
 * 挂载位置决定何时可瞬移；地面与空中都要支持时，同时挂在行走与下落模式上即可。
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UChaosCharacterBlinkCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UE_API UChaosCharacterBlinkCheck(const FObjectInitializer& ObjectInitializer);

	/** 瞬移期间所处的运动模式。切到空中模式可避开地面摩擦与台阶处理对轨迹的干扰 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blink")
	FName TransitionToMode = FName("Falling");

	/** 位移总时长（毫秒）。过短会让碰撞扫描来不及分步、贴墙时更容易被挡停 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blink", meta = (ClampMin = "1", UIMin = "50", UIMax = "400", ForceUnits = "ms"))
	float BlinkDurationMs = 150.f;

	/** 时间重映射指数：小于 1 起步快、末端收得缓，等于 1 为匀速 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blink", meta = (ClampMin = "0.01", UIMin = "0.3", UIMax = "2"))
	float EaseExponent = 0.5f;

	//~Begin UBaseMovementModeTransition Interface
	/** 输入包请求瞬移且终点有效时切换到 TransitionToMode */
	UE_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;

	/** 以本帧位置为起点、输入包终点为终点入队瞬移效果 */
	UE_API virtual void Trigger_Implementation(const FSimulationTickParams& Params) override;
	//~End UBaseMovementModeTransition Interface
};

#undef UE_API
