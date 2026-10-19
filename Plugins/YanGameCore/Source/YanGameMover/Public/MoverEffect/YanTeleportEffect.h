#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"

#include "YanTeleportEffect.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 传送并在同一效果内决定出口速度，供传送门一类的世界 Actor 使用。
 *
 * 引擎 FTeleportEffect 会把入口速度按传送的旋转差转到出口方向；传送门通常要求以固定的
 * 出口速度把人送出去，故在此可选地覆盖。不能拆成「传送效果 + 速度效果」两次入队：
 * FChaosCharacterApplyVelocityEffect 从 StartState 读位置再整体写回 OutputState，
 * 同帧刚完成的传送位置会被它抹回原地。
 *
 * 仅在传送真正发生时才覆盖速度；目标点被占据等原因导致 CanTeleport 失败时，Pawn 原地不动，
 * 此时再改速度只会把人朝出口方向凭空推走。
 */
USTRUCT(BlueprintType)
struct FYanTeleportEffect : public FTeleportEffect
{
	GENERATED_BODY()

	FYanTeleportEffect() = default;
	virtual ~FYanTeleportEffect() = default;

	/** true 则传送成功后把速度覆盖为 VelocityAfterTeleport；false 保留引擎默认的旋转后速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bOverrideVelocityAfterTeleport = false;

	/** 传送成功后的速度（世界空间，cm/s），仅 bOverrideVelocityAfterTeleport 为 true 时生效 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector VelocityAfterTeleport = FVector::ZeroVector;

	//~Begin FInstantMovementEffect Interface
	UE_API virtual bool ApplyMovementEffect_Async(FApplyMovementEffectParams_Async& ApplyEffectParams, FMoverSyncState& OutputState) override;
	UE_API virtual FInstantMovementEffect* Clone() const override;
	UE_API virtual void NetSerialize(FArchive& Ar) override;
	UE_API virtual UScriptStruct* GetScriptStruct() const override;
	UE_API virtual FString ToSimpleString() const override;
	//~End FInstantMovementEffect Interface
};

template <>
struct TStructOpsTypeTraits<FYanTeleportEffect> : public TStructOpsTypeTraitsBase2<FYanTeleportEffect>
{
	enum
	{
		WithCopy = true
	};
};

#undef UE_API
