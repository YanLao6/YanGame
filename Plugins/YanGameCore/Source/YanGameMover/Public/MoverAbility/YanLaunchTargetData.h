#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "YanLaunchTargetData.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 击飞命中数据：携带目标命中结果、世界空间击飞速度和持续时间。
 * 通过 GAS ASC 内建 ServerSetReplicatedTargetData RPC 从客户端传往服务器。
 */
USTRUCT()
struct FYanLaunchTargetData : public FGameplayAbilityTargetData_SingleTargetHit
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize LaunchVelocity = FVector::ZeroVector;

	UPROPERTY()
	float DurationMs = 0.f;

	UE_API virtual TArray<TWeakObjectPtr<AActor>> GetActors() const override;
	virtual UScriptStruct*                 GetScriptStruct() const override { return FYanLaunchTargetData::StaticStruct(); }
	UE_API bool                                   NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template <>
struct TStructOpsTypeTraits<FYanLaunchTargetData> : public TStructOpsTypeTraitsBase2<FYanLaunchTargetData>
{
	enum { WithNetSerializer = true };
};

#undef UE_API
