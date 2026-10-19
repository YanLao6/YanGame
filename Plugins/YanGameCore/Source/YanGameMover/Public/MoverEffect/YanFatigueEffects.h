#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/Character/Effects/ChaosCharacterApplyVelocityEffect.h"
#include "MoverMode/Fatigue/YanFatigueState.h"

#include "YanFatigueEffects.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 带疲劳消耗的速度效果：施加速度之余把消耗记进 FYanFatigueState 对应通道。
 *
 * 跳跃、滑铲前冲等瞬时爆发动作统一排本效果。速度本身由发起方按当前疲劳度换算好后填入，
 * 效果只负责「施加 + 记账」；Transition 的 Trigger 拿不到输出状态，效果是同一 tick 内唯一能写 SyncState 的位置。
 */
USTRUCT(BlueprintInternalUseOnly)
struct FYanFatigueVelocityEffect : public FChaosCharacterApplyVelocityEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	EYanFatigueChannel Channel = EYanFatigueChannel::Jump;

	/** 本次动作增加的疲劳度（0~1） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	float FatigueCost = 0.f;

	/** 该通道每秒恢复的疲劳度，写入状态后由仿真逐步衰减 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	float RecoveryPerSecond = 0.f;

	/** 为 true 时该通道落地后才开始恢复，用于空中动作的冷却 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bRecoverAfterLanding = false;

	//~Begin FInstantMovementEffect Interface
	UE_API virtual bool ApplyMovementEffect_Async(FApplyMovementEffectParams_Async& ApplyEffectParams, FMoverSyncState& OutputState) override;
	UE_API virtual FInstantMovementEffect* Clone() const override;
	UE_API virtual void NetSerialize(FArchive& Ar) override;
	UE_API virtual UScriptStruct* GetScriptStruct() const override;
	UE_API virtual FString ToSimpleString() const override;
	//~End FInstantMovementEffect Interface
};

#undef UE_API
