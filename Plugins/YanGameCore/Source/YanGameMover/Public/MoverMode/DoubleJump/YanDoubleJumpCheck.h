#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/ChaosMovementModeTransition.h"

#include "YanDoubleJumpCheck.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * UYanDoubleJumpCheck - 空中二段跳，把竖直速度重置为 JumpUpwardsSpeed。
 *
 * 读普通跳跃写入的 FCharacterDefaultInputs::bIsJumpJustPressed，只认脉冲上升沿
 * （见 FYanJumpPulseState），起跳所用的脉冲在后续物理步被复用时不会误触发。
 *
 * 冷却记在 FYanFatigueState 的 DoubleJump 通道：使用后通道满值，落地后才按 CooldownSeconds 恢复，
 * 有条目期间不触发。整条判定都在仿真内完成，随 SyncState 回滚，两端结论一致。
 *
 * 「身处空中」由挂载位置保证：本 Transition 只应挂在空中模式（Falling 等）上。
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UYanDoubleJumpCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UE_API UYanDoubleJumpCheck(const FObjectInitializer& ObjectInitializer);

	/** 二段跳后切换到的运动模式，与挂载模式相同即为原地重入 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DoubleJump")
	FName TransitionToMode = FName("Falling");

	/** 二段跳后的竖直速度，与当前下落速度无关，起跳高度恒定 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DoubleJump", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float JumpUpwardsSpeed = 700.f;

	/** 落地后开始计时的冷却时长；为 0 时每次离地都可再用一次 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DoubleJump", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "s"))
	float CooldownSeconds = 5.f;

	//~Begin UBaseMovementModeTransition Interface
	UE_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	UE_API virtual void Trigger_Implementation(const FSimulationTickParams& Params) override;
	//~End UBaseMovementModeTransition Interface
};

#undef UE_API
