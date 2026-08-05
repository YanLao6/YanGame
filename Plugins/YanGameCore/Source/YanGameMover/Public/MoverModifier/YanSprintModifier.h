#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"
#include "NativeGameplayTags.h"

#include "YanSprintModifier.generated.h"

#define UE_API YANGAMEMOVER_API

/** 疾跑状态标签，由本 Modifier 在激活期间对外暴露 */
YANGAMEMOVER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mover_IsSprinting);

/**
 * 疾跑状态修改器：在激活期间覆盖当前角色移动模式的最大速度与加速度。
 *
 * 与引擎 FChaosStanceModifier 同构——疾跑是持续状态而非一次性冲量，
 * 必须由 Modifier 承载：Modifier 随 SyncState 复制并在回滚重放时被完整复现，
 * 而 Transition 的 Trigger 只在切换帧执行一次，其副作用无法在 resim 中重建。
 *
 * 生命周期由 UChaosCharacterSprintCheck 对称管理（DurationMs < 0，需显式取消）。
 */
USTRUCT()
struct FYanSprintModifier : public FMovementModifierBase
{
	GENERATED_BODY()

	UE_API FYanSprintModifier();
	virtual ~FYanSprintModifier() override = default;

	/** 疾跑期间的最大水平速度（cm/s） */
	UPROPERTY()
	float MaxSpeedOverride = 900.0f;

	/** 疾跑期间的加速度（cm/s^2） */
	UPROPERTY()
	float AccelerationOverride = 4000.0f;

	//~Begin FMovementModifierBase Interface
	UE_API virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const override;
	UE_API virtual void GetGameplayTags(FGameplayTagContainer& InOutTags) const override;

	UE_API virtual void OnStart_Async(const FMovementModifierParams_Async& Params) override;
	UE_API virtual void OnEnd_Async(const FMovementModifierParams_Async& Params) override;

	UE_API virtual FMovementModifierBase* Clone() const override;
	UE_API virtual void                   NetSerialize(FArchive& Ar) override;
	UE_API virtual UScriptStruct*         GetScriptStruct() const override;
	UE_API virtual FString                ToSimpleString() const override;
	//~End FMovementModifierBase Interface

protected:
	// 覆盖生效的目标模式；OnEnd 必须清除同一个模式，故不能改用"当前模式"
	FName AppliedModeName = NAME_None;
};

template <>
struct TStructOpsTypeTraits<FYanSprintModifier> : public TStructOpsTypeTraitsBase2<FYanSprintModifier>
{
	enum
	{
		WithCopy = true
	};
};

#undef UE_API
