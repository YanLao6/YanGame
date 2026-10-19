#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"
#include "NativeGameplayTags.h"

#include "YanMaxSpeedScaleModifier.generated.h"

#define UE_API YANGAMEMOVER_API

/** 移速受限状态标签，由本 Modifier 在激活期间对外暴露 */
YANGAMEMOVER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mover_HasMaxSpeedScale);

/**
 * 按比例缩放当前移动模式最大速度的修改器。
 *
 * 用于瞄准、负重一类「在既有速度上打折」的持续状态：与 FYanSprintModifier 覆盖绝对值不同，
 * 本修改器读取激活时的模式速度再乘以系数，因此不需要调用方知道角色的基础移速。
 *
 * 生命周期由排队方持有句柄显式取消（DurationMs < 0）。
 * 与其他覆盖同一模式速度的修改器并存时后启动者生效，结束时一并还原为模式默认值。
 */
USTRUCT()
struct FYanMaxSpeedScaleModifier : public FMovementModifierBase
{
	GENERATED_BODY()

	UE_API FYanMaxSpeedScaleModifier();
	virtual ~FYanMaxSpeedScaleModifier() override = default;

	/** 施加于模式最大速度的倍率 */
	UPROPERTY()
	float SpeedScale = 1.0f;

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
struct TStructOpsTypeTraits<FYanMaxSpeedScaleModifier> : public TStructOpsTypeTraitsBase2<FYanMaxSpeedScaleModifier>
{
	enum
	{
		WithCopy = true
	};
};

#undef UE_API
