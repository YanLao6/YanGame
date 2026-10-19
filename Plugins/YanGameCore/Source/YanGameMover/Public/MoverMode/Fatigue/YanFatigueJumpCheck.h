#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/Character/Transitions/ChaosCharacterJumpCheck.h"

#include "YanFatigueJumpCheck.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * UYanFatigueJumpCheck - 带疲劳的跳跃转换。
 *
 * 起跳速度在父类 JumpUpwardsSpeed（精力充沛）与 MinJumpUpwardsSpeed（完全疲劳）之间按
 * FYanFatigueState 的 Jump 通道插值；每次起跳增加 JumpFatigueCost，随后按 JumpFatigueRecoveryPerSecond 恢复。
 *
 * 父类把速度烘在成员里、效果类型也写死，故 Trigger 整体重写：地面相对速度、
 * 起跳事件与地面反作用冲量与父类保持一致，只把速度来源与效果类型换掉。
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UYanFatigueJumpCheck : public UChaosCharacterJumpCheck
{
	GENERATED_BODY()

public:
	UE_API UYanFatigueJumpCheck(const FObjectInitializer& ObjectInitializer);

	/** 完全疲劳时的起跳速度；精力充沛时取父类 JumpUpwardsSpeed */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Parameters, meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float MinJumpUpwardsSpeed = 350.0f;

	/** 每次起跳增加的疲劳度（0~1） */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Parameters, meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float JumpFatigueCost = 0.25f;

	/** 疲劳度每秒恢复量 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Parameters, meta = (ClampMin = "0", UIMin = "0"))
	float JumpFatigueRecoveryPerSecond = 0.2f;

	//~Begin UBaseMovementModeTransition Interface
	UE_API virtual void Trigger_Implementation(const FSimulationTickParams& Params) override;
	//~End UBaseMovementModeTransition Interface
};

#undef UE_API
