#include "MoverAbility/YanLaunchTargetData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanLaunchTargetData)

TArray<TWeakObjectPtr<AActor>> FYanLaunchTargetData::GetActors() const
{
	return Super::GetActors();
}

bool FYanLaunchTargetData::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	FGameplayAbilityTargetData_SingleTargetHit::NetSerialize(Ar, Map, bOutSuccess);

	bool bVelSuccess = true;
	LaunchVelocity.NetSerialize(Ar, Map, bVelSuccess);

	Ar << DurationMs;

	bOutSuccess = bVelSuccess;
	return true;
}
