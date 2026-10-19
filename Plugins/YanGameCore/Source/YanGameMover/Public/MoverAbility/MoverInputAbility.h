#pragma once

#include "CoreMinimal.h"
#include "MoverSimulationTypes.h"
#include "Abilities/YanGameplayAbility.h"
#include "MoverInputAbility.generated.h"

#define UE_API YANGAMEMOVER_API

class UMoverComponent;

/**
 * Mover 输入技能基类。
 *
 * 技能实例作为输入源登记到 MoverComponent，登记与实例生命周期对齐——Avatar 就绪即
 * 登记，技能被移除才注销，不随激活与结束开关。GAS 的激活期处在 game thread 时间轴，
 * ProduceInput 处在仿真时间轴，两者频率与相位均不一致；若以激活期为登记窗口，
 * 激活当帧写下的脉冲会在下一次仿真采样之前被注销，就此永久丢失。
 *
 * 由此对子类成立两条契约：
 *  - ProduceMoverInput 在技能非激活期间同样每帧调用，实现须在无意图时写入零值。
 *    输入命令包每帧重建，不写与写零值等价，故非激活期的写入不改变任何结果。
 *  - 该回调处于物理注入窗口，其中不得结束技能、生成或销毁 Actor。需要结束时只置位，
 *    真正的 EndAbility 推迟到 game thread。
 */
UCLASS(MinimalAPI, Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UMoverInputAbility : public UYanGameplayAbility, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	//~Begin UGameplayAbility Interface
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	UE_API virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	UE_API virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	//~End UGameplayAbility Interface

	//~Begin UModularGameplayAbility Interface
	UE_API virtual void OnPawnAvatarSet() override;
	//~End UModularGameplayAbility Interface

	//~Begin IMoverInputProducerInterface
	UE_API virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;
	//~End IMoverInputProducerInterface

	/**
	 * 每帧由 MoverComponent 调用（IMoverInputProducerInterface 转发），技能非激活期间同样调用。
	 * 子类在此将技能状态写入 InputCmd 命令包，无意图时须写入零值。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "MoverAbility")
	UE_API void ProduceMoverInput(UMoverComponent* MoverComp, int32 SimTimeMs, UPARAM(ref) FMoverInputCmdContext& InputCmd);

protected:
	/**
	 * 登记的 MoverComponent 变化后调用：首次就绪、Avatar 更换、技能移除时各触发一次。
	 * 子类在此绑定新组件、解绑旧组件的事件。OnPawnAvatarSet 早于登记，其中 CachedMoverComp 尚为空。
	 * OldMoverComp 在技能移除或 Pawn 销毁时可能已失效，使用前须判有效。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "MoverAbility")
	UE_API void OnMoverComponentSet(UMoverComponent* OldMoverComp, UMoverComponent* NewMoverComp);

	/**
	 * 按当前 Avatar 重建输入源登记：解析出的 MoverComponent 与已登记者不同时，先从旧组件
	 * 注销再登记到新组件，Avatar 更换（重生、换 Pawn）由此收敛。
	 * 仅在实例上执行——GAS 对 InstancedPerActor 的 Avatar 类钩子也会在 CDO 上走一遍，
	 * 而 CDO 为所有 Avatar 共用，不得持有任何登记状态。
	 */
	UE_API void RefreshMoverRegistration(const FGameplayAbilityActorInfo* ActorInfo);

	/** 从已登记的 MoverComponent 注销并清空缓存，重复调用安全。 */
	UE_API void UnregisterFromMover();

private:
	// 切换登记目标并通知子类，目标不变时不做任何事
	void SetMoverComponent(UMoverComponent* NewMoverComp);

protected:

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UMoverComponent> CachedMoverComp;
};

#undef UE_API
