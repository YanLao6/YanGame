// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilities/ModularGameplayAbility.h"

#include "YanGameplayAbility.generated.h"

/**
 * 项目技能基类，为 AngelScript 子类提供统一的激活流程钩子。
 *
 * 与 GAS 默认流程的差别：cost 在激活时扣除，冷却则推迟到技能结束时才施加。
 * 因此子类可在 ActivateAbility 中做前置判断，不满足时直接 EndAbility——
 * 那次结束早于冷却标记建立，不会进入冷却。对按住型技能而言，
 * 冷却从松手起算也比从按下起算更符合手感。
 */
UCLASS()
class YANGAMEABILITY_API UYanGameplayAbility : public UModularGameplayAbility
{
	GENERATED_BODY()

public:
	UYanGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	//~End UGameplayAbility Interface

	/** 输入松开时转发，子类通常在此调用 K2_EndAbility()。 */
	UFUNCTION(BlueprintNativeEvent, Category = Ability)
	void K2_InputReleased();

private:
	// 本次激活是否需要在结束时施加冷却；仅当技能真正运行起来后才置位
	bool bCooldownPendingOnEnd = false;
};
