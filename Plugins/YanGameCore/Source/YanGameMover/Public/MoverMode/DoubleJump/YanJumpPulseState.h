#pragma once

#include "CoreMinimal.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"

#include "YanJumpPulseState.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 「上一物理步的输入包带有跳跃脉冲」标记，以在集合中存在与否表达，无数据字段。
 *
 * 异步物理在一帧内多步复用同一份输入包，起跳所用的 bIsJumpJustPressed 会在下一步原样出现，
 * 而那时角色已切入空中模式；空中读跳跃脉冲的 Transition 须据此只认脉冲的上升沿，
 * 否则起跳当帧即被误判为二段跳。
 *
 * 由 UYanChaosMoverSimulation::OnPostSimulationTick 按本步输入包维护：有脉冲则加入，无则移除，
 * 平时不在集合中，不占带宽。放 SyncState 而非黑板，回滚重模拟时才会随状态一起重放。
 */
USTRUCT(BlueprintType)
struct FYanJumpPulseState : public FMoverDataStructBase
{
	GENERATED_USTRUCT_BODY()

	/** 上一物理步是否带有跳跃脉冲 */
	static UE_API bool WasJumpPressedLastStep(const FMoverSyncState& SyncState);

	//~Begin FMoverDataStructBase Interface
	UE_API virtual FMoverDataStructBase* Clone() const override;
	UE_API virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	UE_API virtual UScriptStruct* GetScriptStruct() const override;
	UE_API virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	UE_API virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
	//~End FMoverDataStructBase Interface
};

template <>
struct TStructOpsTypeTraits<FYanJumpPulseState> : public TStructOpsTypeTraitsBase2<FYanJumpPulseState>
{
	enum
	{
		WithNetSerializer = true
	};
};

#undef UE_API
