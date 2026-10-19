#include "MoverEffect/YanWorldTriggerEffects.h"

#include "MoverMode/WorldTrigger/YanWorldTriggerState.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanWorldTriggerEffects)

bool FYanWorldLaunchEffect::ApplyMovementEffect_Async(FApplyMovementEffectParams_Async& ApplyEffectParams, FMoverSyncState& OutputState)
{
	// 先记序号再施加：施加失败（缺 SyncState/Simulation）时脉冲同样视为已消费，不留给下一步重试
	OutputState.SyncStateCollection.FindOrAddMutableDataByType<FYanWorldTriggerState>().LastLaunchRequestId = RequestId;

	return Super::ApplyMovementEffect_Async(ApplyEffectParams, OutputState);
}

FInstantMovementEffect* FYanWorldLaunchEffect::Clone() const
{
	return new FYanWorldLaunchEffect(*this);
}

void FYanWorldLaunchEffect::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	Ar << RequestId;
}

UScriptStruct* FYanWorldLaunchEffect::GetScriptStruct() const
{
	return FYanWorldLaunchEffect::StaticStruct();
}

FString FYanWorldLaunchEffect::ToSimpleString() const
{
	return FString::Printf(TEXT("YanWorldLaunch #%u velocity %s"), RequestId, *VelocityOrImpulseToApply.ToCompactString());
}

bool FYanWorldTeleportEffect::ApplyMovementEffect_Async(FApplyMovementEffectParams_Async& ApplyEffectParams, FMoverSyncState& OutputState)
{
	OutputState.SyncStateCollection.FindOrAddMutableDataByType<FYanWorldTriggerState>().LastTeleportRequestId = RequestId;

	return Super::ApplyMovementEffect_Async(ApplyEffectParams, OutputState);
}

FInstantMovementEffect* FYanWorldTeleportEffect::Clone() const
{
	return new FYanWorldTeleportEffect(*this);
}

void FYanWorldTeleportEffect::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	Ar << RequestId;
}

UScriptStruct* FYanWorldTeleportEffect::GetScriptStruct() const
{
	return FYanWorldTeleportEffect::StaticStruct();
}

FString FYanWorldTeleportEffect::ToSimpleString() const
{
	return FString::Printf(TEXT("YanWorldTeleport #%u to %s"), RequestId, *TargetLocation.ToCompactString());
}
