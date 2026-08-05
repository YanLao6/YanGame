// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include "PhysicalMaterialWithTags.generated.h"

class UObject;

/**
 * 带 GameplayTag 的 PhysicalMaterial。
 * 物理材质除命中反馈外，可由逻辑层根据 Tags 判断表面类型（如脚步、弹痕音效）。
 */
UCLASS(MinimalAPI)
class UPhysicalMaterialWithTags : public UPhysicalMaterial
{
	GENERATED_BODY()

public:
	UPhysicalMaterialWithTags(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 描述该 PhysicalMaterial 的 GameplayTag 集合（Query 用）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=PhysicalProperties)
	FGameplayTagContainer Tags;
};
