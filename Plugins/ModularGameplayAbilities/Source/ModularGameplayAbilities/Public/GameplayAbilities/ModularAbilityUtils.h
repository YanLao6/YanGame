// Copyright Chronicler.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ModularAbilityUtils.generated.h"

#define UE_API MODULARGAMEPLAYABILITIES_API

class UAbilitySystemComponent;
class UGameplayAbility;

/**
 * 按 AssetTags 检索 GameplayAbility 的工具库。
 *
 * 匹配一律包含父级：传入 A.B 可命中声明了 A.B.C 的技能。
 */
UCLASS(MinimalAPI)
class UModularAbilityUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 取技能实例声明的 AssetTags。 */
	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	static UE_API FGameplayTagContainer GetAbilityAssetTags(const UGameplayAbility* Ability);

	/** 取技能类默认对象声明的 AssetTags，供技能尚未授予时查询。 */
	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	static UE_API FGameplayTagContainer GetAbilityClassAssetTags(TSubclassOf<UGameplayAbility> AbilityClass);

	/** 技能是否声明了指定 AssetTag。 */
	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	static UE_API bool AbilityHasAssetTag(const UGameplayAbility* Ability, FGameplayTag AssetTag);

	/**
	 * 按 AssetTag 检索已授予的技能，命中首个即返回。
	 *
	 * @param OutHandle 命中技能的 SpecHandle；未命中时保持无效
	 * @return 是否命中
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tags")
	static UE_API bool FindAbilityHandleByAssetTag(UAbilitySystemComponent* AbilitySystemComponent, FGameplayTag AssetTag, FGameplayAbilitySpecHandle& OutHandle);

	/** 按 AssetTag 集合检索已授予的技能，返回全部命中项。 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tags")
	static UE_API void FindAbilityHandlesByAssetTags(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayTagContainer& AssetTags, TArray<FGameplayAbilitySpecHandle>& OutHandles);

	/** 检索全部已授予技能的 SpecHandle，不作 Tag 过滤。 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tags")
	static UE_API void GetAllAbilityHandles(UAbilitySystemComponent* AbilitySystemComponent, TArray<FGameplayAbilitySpecHandle>& OutHandles);
};

#undef UE_API
