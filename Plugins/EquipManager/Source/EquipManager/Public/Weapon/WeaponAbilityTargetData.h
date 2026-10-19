// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Abilities/GameplayAbilityTargetTypes.h"

#include "WeaponAbilityTargetData.generated.h"

#define UE_API EQUIPMANAGER_API

class FArchive;

/**
 * 携带弹药批次标识的单目标命中数据。
 *
 * 霰弹枪一次击发会产生多条独立射线，它们共享同一个 CartridgeID，
 * 便于接收端把同批次的多个命中合并成一次表现或一次命中反馈。
 */
USTRUCT()
struct FWeaponAbilityTargetData_SingleTargetHit : public FGameplayAbilityTargetData_SingleTargetHit
{
	GENERATED_BODY()

	FWeaponAbilityTargetData_SingleTargetHit()
		: CartridgeID(-1)
	{
	}

	/** 同一次击发产生的所有子弹共享的批次标识，-1 表示未分配。 */
	UPROPERTY()
	int32 CartridgeID;

	//~Begin FGameplayAbilityTargetData Interface
	UE_API virtual void AddTargetDataToContext(FGameplayEffectContextHandle& Context, bool bIncludeActorArray) const override;

	UE_API bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FWeaponAbilityTargetData_SingleTargetHit::StaticStruct();
	}
	//~End FGameplayAbilityTargetData Interface
};

template<>
struct TStructOpsTypeTraits<FWeaponAbilityTargetData_SingleTargetHit> : public TStructOpsTypeTraitsBase2<FWeaponAbilityTargetData_SingleTargetHit>
{
	enum
	{
		// FGameplayAbilityTargetDataHandle 的网络序列化要求子类型必须提供 NetSerialize
		WithNetSerializer = true
	};
};

#undef UE_API
