#include "MoverMode/DoubleJump/YanJumpPulseState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanJumpPulseState)

bool FYanJumpPulseState::WasJumpPressedLastStep(const FMoverSyncState& SyncState)
{
	return SyncState.SyncStateCollection.FindDataByType<FYanJumpPulseState>() != nullptr;
}

FMoverDataStructBase* FYanJumpPulseState::Clone() const
{
	return new FYanJumpPulseState(*this);
}

bool FYanJumpPulseState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	return Super::NetSerialize(Ar, Map, bOutSuccess);
}

UScriptStruct* FYanJumpPulseState::GetScriptStruct() const
{
	return StaticStruct();
}

bool FYanJumpPulseState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	// 无数据字段，两端都存在即一致；存在与否的差异由集合层比对
	return false;
}

void FYanJumpPulseState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	// 无数据字段，存在与否由集合层按 From/To 决定
}
