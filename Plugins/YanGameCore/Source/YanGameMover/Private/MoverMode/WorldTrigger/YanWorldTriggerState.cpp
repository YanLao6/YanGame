#include "MoverMode/WorldTrigger/YanWorldTriggerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanWorldTriggerState)

FMoverDataStructBase* FYanWorldTriggerState::Clone() const
{
	return new FYanWorldTriggerState(*this);
}

bool FYanWorldTriggerState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	Ar << LastLaunchRequestId;
	Ar << LastTeleportRequestId;
	bOutSuccess = true;
	return true;
}

UScriptStruct* FYanWorldTriggerState::GetScriptStruct() const
{
	return StaticStruct();
}

bool FYanWorldTriggerState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FYanWorldTriggerState& Auth = static_cast<const FYanWorldTriggerState&>(AuthorityState);
	return Auth.LastLaunchRequestId != LastLaunchRequestId
	       || Auth.LastTeleportRequestId != LastTeleportRequestId;
}

void FYanWorldTriggerState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	// 序号无插值语义，按 Pct 取离散值
	const FYanWorldTriggerState& Src = static_cast<const FYanWorldTriggerState&>((Pct < 0.5f) ? From : To);
	LastLaunchRequestId   = Src.LastLaunchRequestId;
	LastTeleportRequestId = Src.LastTeleportRequestId;
}

void FYanWorldTriggerState::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);
	Out.Appendf("LastLaunchRequestId: %u | LastTeleportRequestId: %u\n", LastLaunchRequestId, LastTeleportRequestId);
}
