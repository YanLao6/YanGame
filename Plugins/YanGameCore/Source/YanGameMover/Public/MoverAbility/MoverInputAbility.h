#pragma once

#include "CoreMinimal.h"
#include "MoverSimulationTypes.h"
#include "GameplayAbilities/ModularGameplayAbility.h"
#include "MoverInputAbility.generated.h"

class UMoverComponent;

/**
 * Mover 输入技能基类。
 * 
 * CachedMoverComp
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class YANGAMEMOVER_API UMoverInputAbility : public UModularGameplayAbility, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	//~Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	//~End UGameplayAbility Interface

	//~Begin IMoverInputProducerInterface
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;
	//~End IMoverInputProducerInterface

	/** 输入按下时转发，子类可记录帧间状态。 */
	UFUNCTION(BlueprintNativeEvent, Category = "MoverAbility")
	void K2_InputPressed();

	/** 输入松开时转发，子类通常在此调用 K2_EndAbility()。 */
	UFUNCTION(BlueprintNativeEvent, Category = "MoverAbility")
	void K2_InputReleased();

	/**
	 * 每帧由 MoverComponent 调用（IMoverInputProducerInterface 转发）。
	 * 子类在此将技能状态写入 InputCmd 命令包。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "MoverAbility")
	void ProduceMoverInput(UMoverComponent* MoverComp, int32 SimTimeMs, UPARAM(ref) FMoverInputCmdContext& InputCmd);

protected:
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UMoverComponent> CachedMoverComp;
};
