#pragma once

#include "CoreMinimal.h"
#include "MoverSimulationTypes.h"
#include "Abilities/YanGameplayAbility.h"
#include "MoverInputAbility.generated.h"

class UMoverComponent;

/**
 * Mover 输入技能基类。
 *
 * 激活与输入的钩子由 UYanGameplayAbility 统一提供，本类只补 Mover 特有的部分：
 * CachedMoverComp 在 Pawn Avatar 就绪时即建立，故子类可在 ActivateAbility 中
 * 直接据其判断移动状态（如「仅限空中」），不满足时 EndAbility 且不进入冷却；
 * ProduceMoverInput 则在激活期间每帧把技能状态写入输入命令包。
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class YANGAMEMOVER_API UMoverInputAbility : public UYanGameplayAbility, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	//~Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	//~End UGameplayAbility Interface

	//~Begin IMoverInputProducerInterface
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;
	//~End IMoverInputProducerInterface

	/**
	 * 每帧由 MoverComponent 调用（IMoverInputProducerInterface 转发）。
	 * 子类在此将技能状态写入 InputCmd 命令包。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "MoverAbility")
	void ProduceMoverInput(UMoverComponent* MoverComp, int32 SimTimeMs, UPARAM(ref) FMoverInputCmdContext& InputCmd);

protected:
	/**
	 * 从 Avatar 上解析 MoverComponent 并写入 CachedMoverComp，重复调用安全。
	 * ActorInfo 一律由调用方传入而非取自 GetCurrentActorInfo()——GAS 对非
	 * InstancedPerActor 的技能会在 CDO 上回调这些时机，CDO 无 CurrentActorInfo。
	 * 出于同一原因，非实例对象上不做缓存，避免 CDO 被污染后波及所有实例。
	 */
	void CacheMoverComponent(const FGameplayAbilityActorInfo* ActorInfo);

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UMoverComponent> CachedMoverComp;
};
