// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapon/WeaponAbilityTargetData.h"

#include "GameplayAbilities/ModularGameplayEffectContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WeaponAbilityTargetData)

void FWeaponAbilityTargetData_SingleTargetHit::AddTargetDataToContext(FGameplayEffectContextHandle& Context, bool bIncludeActorArray) const
{
	FGameplayAbilityTargetData_SingleTargetHit::AddTargetDataToContext(Context, bIncludeActorArray);

	// 把弹药批次标识带进效果上下文，供表现层合并同批次命中
	if (FModularGameplayEffectContext* TypedContext = FModularGameplayEffectContext::ExtractEffectContext(Context))
	{
		TypedContext->CartridgeID = CartridgeID;
	}
}

bool FWeaponAbilityTargetData_SingleTargetHit::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	FGameplayAbilityTargetData_SingleTargetHit::NetSerialize(Ar, Map, bOutSuccess);

	Ar << CartridgeID;

	return true;
}
