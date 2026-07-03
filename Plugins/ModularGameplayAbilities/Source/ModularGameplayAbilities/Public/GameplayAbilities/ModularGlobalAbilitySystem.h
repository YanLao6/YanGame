// Copyright Chronicler.

#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayAbilitySpecHandle.h"
#include "ActorComponent/ModularAbilitySystemComponent.h"
#include "Templates/SubclassOf.h"

#include "ModularGlobalAbilitySystem.generated.h"

/** 记录已向各 Modular ASC 授予的全局 Ability 实例 Handle。 */
USTRUCT()
struct FGlobalAppliedAbilityList
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<TObjectPtr<UModularAbilitySystemComponent>, FGameplayAbilitySpecHandle> Handles;

	/** 向指定 ASC 授予 Ability（若已有则先移除再授予）。 */
	void AddToAbilityComponent(TSubclassOf<UGameplayAbility> Ability, UModularAbilitySystemComponent* AbilityComponent);
	/** 从指定 ASC 移除本列表中记录的 Ability。 */
	void RemoveFromAbilityComponent(UModularAbilitySystemComponent* AbilityComponent);
	/** 从所有已注册 ASC 上移除 Ability。 */
	void RemoveFromAll();
};

/** 记录已向各 Modular ASC 授予的全局 GameplayEffect Handle。 */
USTRUCT()
struct FGlobalAppliedEffectList
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<TObjectPtr<UModularAbilitySystemComponent>, FActiveGameplayEffectHandle> Handles;

	/** 向指定 ASC 应用 Effect（若已有则先移除再应用）。 */
	void AddToAbilityComponent(TSubclassOf<UGameplayEffect> Effect, UModularAbilitySystemComponent* AbilityComponent);
	/** 从指定 ASC 移除本列表中记录的 Effect。 */
	void RemoveFromAbilityComponent(UModularAbilitySystemComponent* AbilityComponent);
	/** 从所有已注册 ASC 上移除 Effect。 */
	void RemoveFromAll();
};

/**
 * WorldSubsystem：向所有已注册的 Modular ASC 统一施加/回收全局 Ability 与 GameplayEffect。
 */
UCLASS()
class UModularGlobalAbilitySystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 构造。 */
	UModularGlobalAbilitySystem();

	/** Authority：把 Ability 加入“全局列表”，并对已注册 ASC 立即 GiveAbility。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Modular")
	void ApplyAbilityToAll(TSubclassOf<UGameplayAbility> Ability);

	/** Authority：把 GameplayEffect 加入“全局列表”，并对已注册 ASC 立即 Apply。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Modular")
	void ApplyEffectToAll(TSubclassOf<UGameplayEffect> Effect);

	/** Authority：移除某类全局 Ability 及其在所有 ASC 上的实例。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Modular")
	void RemoveAbilityFromAll(TSubclassOf<UGameplayAbility> Ability);

	/** Authority：移除某类全局 GameplayEffect 及其在所有 ASC 上的激活。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Modular")
	void RemoveEffectFromAll(TSubclassOf<UGameplayEffect> Effect);

	/** ASC 具备 Avatar 后注册：补应用当前全局 Ability/Effect。 */
	void RegisterAbilityComponent(UModularAbilitySystemComponent* AbilityComponent);

	/** ASC 销毁或失活时注销：移除由本系统施加的 Ability/Effect。 */
	void UnregisterAbilityComponent(UModularAbilitySystemComponent* AbilityComponent);

private:
	UPROPERTY()
	TMap<TSubclassOf<UGameplayAbility>, FGlobalAppliedAbilityList> AppliedAbilities;

	UPROPERTY()
	TMap<TSubclassOf<UGameplayEffect>, FGlobalAppliedEffectList> AppliedEffects;

	UPROPERTY()
	TArray<TObjectPtr<UModularAbilitySystemComponent>> RegisteredAbilityComponents;
};
