#pragma once

#include "GameplayAbilities/ModularAbilitySet.h"
#include "GameplayAbilities/ModularAbilityTagRelationshipMapping.h"

#include "IAbilityPawnDataInterface.generated.h"

UINTERFACE(MinimalAPI, NotBlueprintable)
class UAbilityPawnDataInterface: public UInterface
{
	GENERATED_BODY()
};

/**
 * 为 PawnData 扩展 Ability 配置（AbilitySet、TagRelationshipMapping）。
 */
class MODULARGAMEPLAYABILITIES_API IAbilityPawnDataInterface
{
	GENERATED_BODY()

public:
	/**
	 * 返回 PawnData 上的 AbilitySet 列表。
	 *
	 * 典型字段示例：`TArray<TObjectPtr<UModularAbilitySet>> AbilitySets;`
	 */
	virtual TArray<UModularAbilitySet*> GetAbilitySet() const;

	/**
	 * 返回 Tag 关系映射资产指针。
	 *
	 * 典型字段示例：`TObjectPtr<UModularAbilityTagRelationshipMapping> TagRelationshipMapping;`
	 */
	virtual UModularAbilityTagRelationshipMapping* GetTagRelationshipMapping() const;
};
