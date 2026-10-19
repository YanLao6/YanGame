#pragma once

#include "CoreMinimal.h"
#include "Abilities/YanGameplayAbility.h"
#include "NativeGameplayTags.h"

#include "YanBreakAbility.generated.h"

#define UE_API YANGAMEMOVER_API

class USoundBase;

/**
 * 卸力窗口：受击后短暂存在，是卸力技能唯一的激活条件。
 * 由击退 Execution 在施加击退的同一帧挂到目标身上，窗口 GE 的持续时间即窗口长度。
 */
YANGAMEMOVER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Break_Window);

/**
 * 卸力：受击后的窗口内花一格机动能量，把身上这股击退卸掉一部分并偏个方向。
 *
 * 它是全场唯一的防守手段，因而必须收费——不收费则击退永远杀不了人，
 * 不存在则先手方永远赢。收费的后果串起本作的核心闭环：
 * 被打掉能量 → 无法卸力 → 被打得更远，「连段」由资源而非帧数构成。
 *
 * 窗口做成 0.25 秒而非几帧，是联机架构逼出来的：预测回滚下客户端看到的受击时刻
 * 与服务器判定存在偏差，要求帧级精度的防守动作在实际网络下只能是玄学。
 * 限制使用频率的是能量，不是时机精度。
 *
 * 时序上卸力发生在击退**之后**而非拦截击退：击退当帧由服务器结算完毕，
 * 玩家看到自己飞出去，再在窗口内按键改写这股速度。故本技能读的是当前速度，
 * 不需要知道是谁、用什么把自己打飞的。
 *
 * 网络模型：消耗走 LocalPredicted，按下即扣、HUD 立刻响应；速度改写走服务器权威
 * sim action，与击退本身同一通道——两者若一个预测一个权威，会在同一条速度上打架。
 *
 * 蓝图默认值需设置：
 *   CostGameplayEffectClass → 扣 1 格 Charge 的消耗 GE，留空则卸力免费，闭环不成立
 *   ActivationRequiredTags  → 须含 Status.Break.Window，否则任意时刻都能卸力
 */
UCLASS(MinimalAPI)
class UYanBreakAbility : public UYanGameplayAbility
{
	GENERATED_BODY()

public:
	UE_API explicit UYanBreakAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UGameplayAbility Interface
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~End UGameplayAbility Interface

protected:
	/** 卸掉的击退比例：0.4 表示速度只剩六成 */
	UPROPERTY(EditDefaultsOnly, Category = "Break", Meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpeedReduction = 0.4f;

	/**
	 * 朝移动输入方向的最大偏转角（度）。
	 * 上限存在的意义是让卸力只能「拐一点」而非原地改向——后者等于取消击退。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Break", Meta = (ClampMin = "0.0", ClampMax = "180.0", ForceUnits = "degrees"))
	float MaxDeflectionDegrees = 35.f;

	/** 低于此速度视为没有可卸之力，不生效也不退费 */
	UPROPERTY(EditDefaultsOnly, Category = "Break", Meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float MinBreakableSpeed = 50.f;

	UPROPERTY(EditDefaultsOnly, Category = "Break|Feedback")
	TObjectPtr<USoundBase> BreakSound;

	/** 蓝图实现：卸力生效瞬间的本地表现 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Break")
	UE_API void OnBreakPerformed();

private:
	// 服务器权威：改写自身速度并消费掉窗口，使一次受击只能卸一次
	UE_API void ApplyBreakToSelf() const;

	// 按 SpeedReduction 缩短，再朝移动输入方向偏转不超过 MaxDeflectionDegrees
	UE_API FVector ComputeBrokenVelocity(const FVector& CurrentVelocity, const FVector& DeflectDirection) const;

	UE_API void PlayLocalFeedback() const;
};

#undef UE_API
