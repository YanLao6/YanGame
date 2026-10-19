// Copyright Chronicler.

#pragma once

#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"

#include "ModularAbilityGrant.generated.h"

#define UE_API MODULARGAMEPLAYABILITIES_API

class UAttributeSet;
class UDataTable;
class UGameplayAbility;
class UGameplayEffect;
class UModularAbilitySet;
class UModularAbilitySystemComponent;
struct FModularAbilitySet_GrantedHandles;

/**
 * 散装授予的单个 GameplayAbility，字段与 AbilitySet 条目一致，但以软引用配置。
 */
USTRUCT(BlueprintType)
struct FModularAbilityGrant
{
	GENERATED_BODY()

	// 要授予的 GameplayAbility 类。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Client,Server"))
	TSoftClassPtr<UGameplayAbility> AbilityType;

	// 授予时的 AbilityLevel。
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AbilityLevel = 1;

	// 用于把 Input 映射到 Ability 的 GameplayTag，可留空。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/**
 * 散装授予的单个 GameplayEffect。
 */
USTRUCT(BlueprintType)
struct FModularEffectGrant
{
	GENERATED_BODY()

	// 要应用到自身的 GameplayEffect 类。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Client,Server"))
	TSoftClassPtr<UGameplayEffect> EffectType;

	// 应用时的 EffectLevel。
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float EffectLevel = 1.0f;
};

/**
 * 散装授予的单个 AttributeSet，可附带初始化数据表。
 */
USTRUCT(BlueprintType)
struct FModularAttributeSetGrant
{
	GENERATED_BODY()

	// 要授予的 AttributeSet 类。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Client,Server"))
	TSoftClassPtr<UAttributeSet> AttributeSetType;

	// 用于初始化属性的数据表，可留空。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Client,Server"))
	TSoftObjectPtr<UDataTable> InitializationData;
};

namespace ModularAbilityGrant
{
	/**
	 * 将散装条目与 AbilitySet 一并授予目标 ASC，产生的 Handle 全部追加进 OutGrantedHandles，
	 * 由其 TakeFromAbilitySystem 统一回收。须在 Authority 上调用，否则不做任何事。
	 *
	 * 授予顺序为 AttributeSet → Ability → Effect → AbilitySet，保证 Effect 应用时属性已存在。
	 */
	UE_API void GiveToAbilitySystem(
		UModularAbilitySystemComponent* ModularASC,
		TConstArrayView<FModularAbilityGrant> Abilities,
		TConstArrayView<FModularEffectGrant> Effects,
		TConstArrayView<FModularAttributeSetGrant> Attributes,
		TConstArrayView<TSoftObjectPtr<const UModularAbilitySet>> AbilitySets,
		FModularAbilitySet_GrantedHandles& OutGrantedHandles);
}

#undef UE_API
