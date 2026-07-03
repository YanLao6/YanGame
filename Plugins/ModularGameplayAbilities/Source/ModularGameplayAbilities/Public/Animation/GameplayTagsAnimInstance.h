// Copyright Chronicler.

#pragma once

#include "Animation/AnimInstance.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagsAnimInstance.generated.h"

/**
 * 在 AnimInstance 上集成 GameplayTag → Blueprint 变量自动同步。
 *
 * 避免在 Animation Blueprint 中轮询 Pawn 的 GameplayTag。
 *
 * FGameplayTagBlueprintPropertyMap 依赖 AbilitySystemComponent 维护 Tag 状态。
 *
 * @todo ASC 解耦后可迁到独立 GameplayTags 插件。
 *
 * @see https://dev.epicgames.com/community/learning/tutorials/n2nJ/unreal-engine-fgameplaytagblueprintpropertymap-the-tag-watcher
 */
UCLASS(Config = Game)
class MODULARGAMEPLAYABILITIES_API UGameplayTagsAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** 构造。 */
	UGameplayTagsAnimInstance(const FObjectInitializer& ObjectInitializer);

	/**
	 * 使用指定 ASC 初始化 Tag 属性映射。
	 *
	 * @param AbilityComponent 目标 UAbilitySystemComponent。
	 *
	 * @todo 与 NativeInitializeAnimation 的路径收敛，避免重复初始化。
	 */
	virtual void InitializeWithAbilitySystem(UAbilitySystemComponent* AbilityComponent);

protected:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	// 可映射到 Blueprint 变量的 GameplayTag；增删 Tag 时自动刷新变量。
	UPROPERTY(EditDefaultsOnly, Category = "GameplayTags")
	FGameplayTagBlueprintPropertyMap GameplayTagPropertyMap;

	UPROPERTY(BlueprintReadOnly, Category = "Character State Data")
	float GroundDistance = -1.0f;
};
