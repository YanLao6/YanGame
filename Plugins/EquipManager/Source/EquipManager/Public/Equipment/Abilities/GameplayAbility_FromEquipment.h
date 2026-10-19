// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayAbilities/ModularGameplayAbility.h"

#include "GameplayAbility_FromEquipment.generated.h"

#define UE_API EQUIPMANAGER_API

class UEquipmentInstance;
class UInventoryItemInstance;

/**
 * 由装备授予并与之关联的能力基类。
 *
 * 授予时把来源写入 Spec 的 SourceObject，因此子类可以在运行时反查自己是从哪件装备来的。
 * 来源有两种：被动由装备实例授予，来源即 UEquipmentInstance；
 * 主动技能在物品入槽时就授予，其 Spec 随 PlayerState 存续而装备实例随 Pawn 销毁，
 * 故来源是 UInventoryItemInstance，装备实例须按当前 Avatar 反查。
 */
UCLASS(MinimalAPI, Abstract)
class UGameplayAbility_FromEquipment : public UModularGameplayAbility
{
	GENERATED_BODY()

public:

	UE_API UGameplayAbility_FromEquipment(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 返回授予该能力的装备实例；能力并非由装备授予时返回空。 */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Ability")
	UE_API UEquipmentInstance* GetAssociatedEquipment() const;

	/** 返回该装备对应的背包物品实例；装备并非由背包物品生成时返回空。 */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Ability")
	UE_API UInventoryItemInstance* GetAssociatedItem() const;

#if WITH_EDITOR
	//~Begin UObject Interface
	UE_API virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
	//~End UObject Interface
#endif

private:
	// 取当前 Spec 记录的来源对象；非实例化能力没有 Spec 上下文，此时返回空。
	UObject* GetAssociatedSourceObject() const;
};

#undef UE_API
