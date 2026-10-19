#include "Abilities/SlashAbilityTargetData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SlashAbilityTargetData)

bool FSlashAbilityTargetData_Geometry::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	SlashOrigin.NetSerialize(Ar, Map, bOutSuccess);
	AimDirection.NetSerialize(Ar, Map, bOutSuccess);
	SlashPlaneNormal.NetSerialize(Ar, Map, bOutSuccess);
	Ar << QuantizedChargeAlpha;

	bOutSuccess = true;
	return true;
}
