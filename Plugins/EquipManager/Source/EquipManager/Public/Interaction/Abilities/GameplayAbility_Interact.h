// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayAbilities/ModularGameplayAbility.h"
#include "Interaction/InteractionOption.h"

#include "GameplayAbility_Interact.generated.h"

class UIndicatorDescriptor;
class UObject;
class UUserWidget;
struct FFrame;
struct FGameplayAbilityActorInfo;
struct FGameplayEventData;

/**
 * 交互能力。
 *
 * 常驻授予（OnSpawn），周期性扫描附近可交互物并生成屏幕指示器，
 * 在玩家确认时触发当前选项对应的交互能力。
 */
UCLASS(MinimalAPI, Abstract)
class UGameplayAbility_Interact : public UModularGameplayAbility
{
	GENERATED_BODY()

public:

	UGameplayAbility_Interact(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~End UGameplayAbility Interface

	/** 用最新的交互选项集合刷新屏幕指示器。 */
	UFUNCTION(BlueprintCallable)
	void UpdateInteractions(const TArray<FInteractionOption>& InteractiveOptions);

	/** 触发当前首选交互选项对应的能力。 */
	UFUNCTION(BlueprintCallable)
	void TriggerInteraction();

protected:
	UPROPERTY(BlueprintReadWrite)
	TArray<FInteractionOption> CurrentOptions;

	UPROPERTY()
	TArray<TObjectPtr<UIndicatorDescriptor>> Indicators;

protected:

	UPROPERTY(EditDefaultsOnly)
	float InteractionScanRate = 0.1f;

	UPROPERTY(EditDefaultsOnly)
	float InteractionScanRange = 500;

	UPROPERTY(EditDefaultsOnly)
	TSoftClassPtr<UUserWidget> DefaultInteractionWidgetClass;
};
