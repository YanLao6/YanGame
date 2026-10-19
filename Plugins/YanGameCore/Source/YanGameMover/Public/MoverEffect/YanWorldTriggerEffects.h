#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/Character/Effects/ChaosCharacterApplyVelocityEffect.h"
#include "MoverEffect/YanTeleportEffect.h"

#include "YanWorldTriggerEffects.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 发射板的速度效果：施加速度之余把请求序号记进 FYanWorldTriggerState，
 * 使同一份输入包在后续物理步里不再重复触发。序号语义见 FYanWorldTriggerState。
 */
USTRUCT(BlueprintInternalUseOnly)
struct FYanWorldLaunchEffect : public FChaosCharacterApplyVelocityEffect
{
	GENERATED_BODY()

	/** 本次发射对应的输入包请求序号 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	uint8 RequestId = 0;

	//~Begin FInstantMovementEffect Interface
	UE_API virtual bool ApplyMovementEffect_Async(FApplyMovementEffectParams_Async& ApplyEffectParams, FMoverSyncState& OutputState) override;
	UE_API virtual FInstantMovementEffect* Clone() const override;
	UE_API virtual void NetSerialize(FArchive& Ar) override;
	UE_API virtual UScriptStruct* GetScriptStruct() const override;
	UE_API virtual FString ToSimpleString() const override;
	//~End FInstantMovementEffect Interface
};

/**
 * 传送门的传送效果：与 FYanWorldLaunchEffect 同一契约。
 * 目标点被占据导致传送未发生时序号仍记为已消费——脉冲只表达一次意图，不做重试。
 */
USTRUCT(BlueprintInternalUseOnly)
struct FYanWorldTeleportEffect : public FYanTeleportEffect
{
	GENERATED_BODY()

	/** 本次传送对应的输入包请求序号 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	uint8 RequestId = 0;

	//~Begin FInstantMovementEffect Interface
	UE_API virtual bool ApplyMovementEffect_Async(FApplyMovementEffectParams_Async& ApplyEffectParams, FMoverSyncState& OutputState) override;
	UE_API virtual FInstantMovementEffect* Clone() const override;
	UE_API virtual void NetSerialize(FArchive& Ar) override;
	UE_API virtual UScriptStruct* GetScriptStruct() const override;
	UE_API virtual FString ToSimpleString() const override;
	//~End FInstantMovementEffect Interface
};

template <>
struct TStructOpsTypeTraits<FYanWorldTeleportEffect> : public TStructOpsTypeTraitsBase2<FYanWorldTeleportEffect>
{
	enum
	{
		WithCopy = true
	};
};

#undef UE_API
