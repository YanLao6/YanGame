// Copyright Chronicler.

#pragma once

#include "UObject/Interface.h"

#include "ModularAbilitySourceInterface.generated.h"

class UObject;
class UPhysicalMaterial;
struct FGameplayTagContainer;

/** 可作为 Ability / GameplayEffect 计算源头（距离衰减、材质衰减等）的对象接口。 */
UINTERFACE()
class UModularAbilitySourceInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IModularAbilitySourceInterface
{
	GENERATED_IINTERFACE_BODY()

	/**
	 * 距离衰减系数（falloff multiplier）。
	 *
	 * @param Distance 源到目标距离（如弹道飞行距离）。
	 * @param SourceTags 源侧聚合 GameplayTag。
	 * @param TargetTags 目标当前 GameplayTag。
	 * @return 乘到基础属性上的系数。
	 */
	virtual float GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr) const = 0;

	/**
	 * 基于 UPhysicalMaterial 的额外衰减（如穿透、表面类型）。
	 */
	virtual float GetPhysicalMaterialAttenuation(const UPhysicalMaterial* PhysicalMaterial, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr) const = 0;
};
