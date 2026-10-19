#pragma once

#include "Abilities/YanGameplayAbility.h"
#include "NativeGameplayTags.h"

#include "YanDeathAbility.generated.h"

#define UE_API YANGAMEPLAY_API

/** 触发死亡的 GameplayEvent；出界判定以此事件通知目标 ASC。 */
YANGAMEPLAY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_Death);

/**
 * 死亡技能：清算并安排重生。
 *
 * 死亡来源可以是生命归零，也可以是被推出场地边界，两者都以 GameplayEvent.Death 触发本技能。
 * 死亡阶段由生命组件承载：激活时进入 DeathStarted，技能结束时推进到 DeathFinished，
 * 由订阅方（Pawn 的销毁收尾、武器的收枪等）各自响应。
 *
 * 特长在此不做任何处理：选定即锁死，且解锁进度挂在 PlayerState 上跨死亡存活。
 *
 * 负荷在此清零而非在重生时清零，是为了让死亡瞬间的 HUD 与材质立刻回到基准色，
 * 避免玩家在等待重生的几秒里仍看到满负荷的自己。
 */
UCLASS(MinimalAPI)
class UYanDeathAbility : public UYanGameplayAbility
{
	GENERATED_BODY()

public:
	UE_API explicit UYanDeathAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UGameplayAbility Interface
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	UE_API virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End UGameplayAbility Interface

protected:
	/**
	 * 是否由本技能安排重生。
	 *
	 * 关闭时本技能只做死亡清算，重生与技能的结束都交给外部的重生技能（如被动的自动重生）负责；
	 * 两边同时安排会各自请求一次重生，玩家会连续复活两次。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	bool bAutoRequestRespawn = true;

	/** 死亡到重生之间的等待时长。 */
	UPROPERTY(EditDefaultsOnly, Category = "Death", meta = (ClampMin = "0.0", ForceUnits = "s", EditCondition = "bAutoRequestRespawn"))
	float RespawnDelay = 3.0f;

private:
	// 推进生命组件的死亡阶段；阶段不匹配时静默跳过，容许技能在任意路径上重复调用
	UE_API void StartDeath() const;
	UE_API void FinishDeath() const;

	// 清零负荷，使重生后从基准状态起算
	UE_API void ResetLoad() const;

	// 把出界告知赛制组件，由其裁决本回合归谁；对抗以外的 Experience 无此组件，静默跳过
	UE_API void NotifyRoundComponent() const;

	// 延时结束后请求 GameMode 重生本控制器
	UE_API void RequestRespawn();

	FTimerHandle RespawnTimerHandle;
};

#undef UE_API
