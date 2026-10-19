#include "MoverEffect/YanTeleportEffect.h"

#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanTeleportEffect)

bool FYanTeleportEffect::ApplyMovementEffect_Async(FApplyMovementEffectParams_Async& ApplyEffectParams, FMoverSyncState& OutputState)
{
	if (!Super::ApplyMovementEffect_Async(ApplyEffectParams, OutputState))
	{
		return false;
	}

	if (!bOverrideVelocityAfterTeleport)
	{
		return true;
	}

	// AttemptTeleport 不返回结果，只发事件；以输出位置是否已到目标点判定传送是否发生
	FMoverDefaultSyncState* OutputSyncState = OutputState.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
	if (!OutputSyncState || !OutputSyncState->GetLocation_WorldSpace().Equals(TargetLocation, UE_KINDA_SMALL_NUMBER))
	{
		return true;
	}

	OutputSyncState->SetTransforms_WorldSpace(OutputSyncState->GetLocation_WorldSpace(),
	                                          OutputSyncState->GetOrientation_WorldSpace(),
	                                          VelocityAfterTeleport,
	                                          OutputSyncState->GetAngularVelocityDegrees_WorldSpace());

	return true;
}

FInstantMovementEffect* FYanTeleportEffect::Clone() const
{
	return new FYanTeleportEffect(*this);
}

void FYanTeleportEffect::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	Ar << bOverrideVelocityAfterTeleport;
	Ar << VelocityAfterTeleport;
}

UScriptStruct* FYanTeleportEffect::GetScriptStruct() const
{
	return FYanTeleportEffect::StaticStruct();
}

FString FYanTeleportEffect::ToSimpleString() const
{
	return FString::Printf(TEXT("YanTeleport to %s, override velocity %s"), *TargetLocation.ToCompactString(), bOverrideVelocityAfterTeleport ? TEXT("on") : TEXT("off"));
}
