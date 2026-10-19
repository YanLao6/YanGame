#pragma once

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/NetSerialization.h"

#include "SlashAbilityTargetData.generated.h"

#define UE_API SHINJUKUSHOWDOWNRUNTIME_API

class FArchive;

/**
 * 一次斩击的几何解算结果。
 *
 * 挥砍朝向来自本地视角采样，权威端无从自行重建，因此随 TargetData 一并复制过去，
 * 用于还原破坏场的位置、朝向与尺寸。整次斩击只产生一份，与命中数量无关。
 */
USTRUCT()
struct FSlashAbilityTargetData_Geometry : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	/** 斩击起点，即解算瞄准时的相机位置。 */
	UPROPERTY()
	FVector_NetQuantize SlashOrigin = FVector::ZeroVector;

	/** 视线方向，同时是破坏场盒子的深度轴。 */
	UPROPERTY()
	FVector_NetQuantizeNormal AimDirection = FVector::ForwardVector;

	/** 挥砍平面法线，同时是破坏场盒子的厚度轴。 */
	UPROPERTY()
	FVector_NetQuantizeNormal SlashPlaneNormal = FVector::UpVector;

	/** 蓄力进度，按 [0,1] 铺满整个字节；深度只是区间插值，该精度已足够。 */
	UPROPERTY()
	uint8 QuantizedChargeAlpha = 0;

	void SetChargeAlpha(float InChargeAlpha)
	{
		QuantizedChargeAlpha = static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(InChargeAlpha, 0.0f, 1.0f) * 255.0f));
	}

	float GetChargeAlpha() const
	{
		return static_cast<float>(QuantizedChargeAlpha) / 255.0f;
	}

	//~Begin FGameplayAbilityTargetData Interface
	UE_API bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FSlashAbilityTargetData_Geometry::StaticStruct();
	}
	//~End FGameplayAbilityTargetData Interface
};

template <>
struct TStructOpsTypeTraits<FSlashAbilityTargetData_Geometry> : public TStructOpsTypeTraitsBase2<FSlashAbilityTargetData_Geometry>
{
	enum
	{
		// FGameplayAbilityTargetDataHandle 的网络序列化要求子类型必须提供 NetSerialize
		WithNetSerializer = true
	};
};

#undef UE_API
