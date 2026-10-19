#pragma once

#include "CoreMinimal.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"

#include "YanWorldTriggerState.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 世界触发器（发射板、传送门）最近一次已消费的请求序号，随 SyncStateCollection 参与 rollback/resim 与复制。
 *
 * 输入包的粒度是 game 帧而非物理步：game 线程卡顿一帧时 Chaos 会连跑多个固定步，
 * 且这几步复用同一份输入包（FSimCallbackInput::SetNumSteps_External）。发射/传送是单帧脉冲，
 * 若只看输入位，每一步都会再触发一次，速度被叠加成步数倍。
 * 故输入包为每次请求附带自增序号，sim 侧记下最近消费的序号，相同序号不再触发。
 *
 * 由效果（FYanWorldLaunchEffect / FYanWorldTeleportEffect）在 ApplyMovementEffect_Async 写入：
 * Transition 的 Trigger 拿不到输出状态，效果是同一 tick 内唯一能写 SyncState 的位置。
 * 放 SyncState 而非黑板：黑板不随重模拟回退，回滚后会漏掉本该重放的脉冲。
 */
USTRUCT(BlueprintType)
struct FYanWorldTriggerState : public FMoverDataStructBase
{
	GENERATED_USTRUCT_BODY()

	/** 最近一次已施加的发射请求序号；0 表示尚未有过请求 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Yan|Mover|WorldTrigger")
	uint8 LastLaunchRequestId = 0;

	/** 最近一次已执行的传送请求序号；0 表示尚未有过请求 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Yan|Mover|WorldTrigger")
	uint8 LastTeleportRequestId = 0;

	/** 该序号的发射请求是否已被 SyncState 中记录的状态消费过；状态尚不存在视为未消费 */
	static bool IsLaunchConsumed(const FMoverSyncState& SyncState, uint8 RequestId)
	{
		const FYanWorldTriggerState* State = SyncState.SyncStateCollection.FindDataByType<FYanWorldTriggerState>();
		return State && State->LastLaunchRequestId == RequestId;
	}

	static bool IsTeleportConsumed(const FMoverSyncState& SyncState, uint8 RequestId)
	{
		const FYanWorldTriggerState* State = SyncState.SyncStateCollection.FindDataByType<FYanWorldTriggerState>();
		return State && State->LastTeleportRequestId == RequestId;
	}

	//~Begin FMoverDataStructBase Interface
	UE_API virtual FMoverDataStructBase* Clone() const override;
	UE_API virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	UE_API virtual UScriptStruct* GetScriptStruct() const override;
	UE_API virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	UE_API virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
	UE_API virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	//~End FMoverDataStructBase Interface
};

template <>
struct TStructOpsTypeTraits<FYanWorldTriggerState> : public TStructOpsTypeTraitsBase2<FYanWorldTriggerState>
{
	enum
	{
		WithNetSerializer = true
	};
};

#undef UE_API
