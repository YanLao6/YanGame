#include "MoverEffect/YanFatigueEffects.h"

#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanFatigueEffects)

bool FYanFatigueVelocityEffect::ApplyMovementEffect_Async(FApplyMovementEffectParams_Async& ApplyEffectParams, FMoverSyncState& OutputState)
{
	// 先记账再施加：施加失败时动作同样视为已发生，不留给下一步重试
	OutputState.SyncStateCollection.FindOrAddMutableDataByType<FYanFatigueState>().Accumulate(Channel, FatigueCost, RecoveryPerSecond, bRecoverAfterLanding);

	return Super::ApplyMovementEffect_Async(ApplyEffectParams, OutputState);
}

FInstantMovementEffect* FYanFatigueVelocityEffect::Clone() const
{
	return new FYanFatigueVelocityEffect(*this);
}

void FYanFatigueVelocityEffect::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	Ar << Channel;
	Ar << FatigueCost;
	Ar << RecoveryPerSecond;
	Ar << bRecoverAfterLanding;
}

UScriptStruct* FYanFatigueVelocityEffect::GetScriptStruct() const
{
	return FYanFatigueVelocityEffect::StaticStruct();
}

FString FYanFatigueVelocityEffect::ToSimpleString() const
{
	return FString::Printf(TEXT("YanFatigueVelocity[%u] cost %.2f velocity %s"), static_cast<uint8>(Channel), FatigueCost, *VelocityOrImpulseToApply.ToCompactString());
}
