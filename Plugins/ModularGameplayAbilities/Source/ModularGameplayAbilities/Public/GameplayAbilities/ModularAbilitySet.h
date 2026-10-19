// Copyright Chronicler.

#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "AttributeSet.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbility.h"
#include "ActorComponent/ModularAbilitySystemComponent.h"

#include "ModularAbilitySet.generated.h"

#define UE_API MODULARGAMEPLAYABILITIES_API

/**
 * AbilitySet 用于授予 AttributeSet 的配置条目。
 */
USTRUCT(BlueprintType)
struct FModularAbilitySet_AttributeSet
{
	GENERATED_BODY()

public:
	// 要授予的 AttributeSet 类型。
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAttributeSet> AttributeSet;
};

/**
 * AbilitySet 用于授予 GameplayAbility 的配置条目。
 */
USTRUCT(BlueprintType)
struct FModularAbilitySet_GameplayAbility
{
	GENERATED_BODY()

public:
	// 要授予的 GameplayAbility 类。
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayAbility> Ability = nullptr;

	// 授予时的 AbilityLevel。
	UPROPERTY(EditDefaultsOnly)
	int32 AbilityLevel = 1;

	// 用于把 Input 映射到 Ability 的 GameplayTag（通常为 InputTag）。
	UPROPERTY(EditDefaultsOnly, Meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};


/**
 * AbilitySet 用于授予 GameplayEffect 的配置条目。
 */
USTRUCT(BlueprintType)
struct FModularAbilitySet_GameplayEffect
{
	GENERATED_BODY()

public:
	// 要授予的 GameplayEffect 类。
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> GameplayEffect = nullptr;

	// 授予时的 EffectLevel。
	UPROPERTY(EditDefaultsOnly)
	float EffectLevel = 1.0f;
};

/**
 * 记录 AbilitySet 已授予内容的 Handle，便于后续统一回收。
 */
USTRUCT(BlueprintType)
struct FModularAbilitySet_GrantedHandles
{
	GENERATED_BODY()

public:
	/** 记录已授予的 AbilitySpecHandle。 */
	UE_API void AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle);
	/** 记录已授予的 GameplayEffect Handle。 */
	UE_API void AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle);
	/** 记录已生成并挂到 ASC 的 AttributeSet 实例。 */
	UE_API void AddAttributeSet(UAttributeSet* Set);

	/** 从指定 ASC 上移除本结构记录的全部授予内容。 */
	UE_API void TakeFromAbilitySystem(UModularAbilitySystemComponent* AbilitySystemComponent);

protected:
	// 已授予 Ability 的 SpecHandle 列表。
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	// 已授予 GameplayEffect 的 Handle 列表。
	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;

	// 已授予的 AttributeSet 实例指针。
	UPROPERTY()
	TArray<TObjectPtr<UAttributeSet>> GrantedAttributeSets;
};

/**
 * GiveToAbilitySystem 的授予范围。
 *
 * 技能可以在物品进入快捷栏时就提前授予，使其 SpecHandle 早于装备生效抵达客户端；
 * 而被动部分一旦授予即刻产生数值影响，必须等到装备真正生效才能应用。
 * 两者生效时机不同，故需要分别授予。
 */
UENUM(BlueprintType)
enum class EModularAbilitySetGrantParts : uint8
{
	// 技能与被动一并授予。
	All,
	// 仅授予 GameplayAbility。
	AbilitiesOnly,
	// 仅授予 GameplayEffect 与 AttributeSet。
	PassivesOnly,
};

/**
 * Modular AbilitySet 数据资产。
 *
 * 用于以数据驱动方式批量授予 GameplayAbility、GameplayEffect 与 AttributeSet。
 */
UCLASS(MinimalAPI, BlueprintType, Const)
class UModularAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 构造 AbilitySet 资产。 */
	UE_API UModularAbilitySet(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * 将本 AbilitySet 授予目标 ASC。
	 *
	 * @param ModularASC 目标 ModularAbilitySystemComponent（需在 Owner 上具备 Authority）。
	 * @param OutGrantedHandles 可选；用于回传本次授予产生的 Handle，供后续 Take 回收。
	 * @param SourceObject 写入 AbilitySpec.SourceObject，便于追溯来源。
	 * @param Parts 本次授予的范围；分两次授予时，两次的 Handle 可回收进同一个 OutGrantedHandles。
	 * @param GateTag 可选；写入各 AbilitySpec 的门禁 Tag（须位于 Ability.Gate 之下），
	 *                持有者不拥有该 Tag 时这些技能无法激活。与授予同步写入，故不存在无门禁的窗口。
	 */
	UE_API void GiveToAbilitySystem(UModularAbilitySystemComponent* ModularASC, FModularAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject = nullptr, EModularAbilitySetGrantParts Parts = EModularAbilitySetGrantParts::All, FGameplayTag GateTag = FGameplayTag()) const;

protected:
	// 授予时发放的 GameplayAbility 列表。
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Abilities", meta=(TitleProperty=Ability))
	TArray<FModularAbilitySet_GameplayAbility> GrantedGameplayAbilities;

	// 授予时发放的 GameplayEffect 列表。
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Effects", meta=(TitleProperty=GameplayEffect))
	TArray<FModularAbilitySet_GameplayEffect> GrantedGameplayEffects;

	// 授予时挂接的 AttributeSet 类型列表。
	UPROPERTY(EditDefaultsOnly, Category = "Attribute Sets", meta=(TitleProperty=AttributeSet))
	TArray<FModularAbilitySet_AttributeSet> GrantedAttributes;
};

#undef UE_API
