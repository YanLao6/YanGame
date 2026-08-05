/**
 * UAS_QuickBarSetSlot — 切换到指定快捷栏槽位（发送方）。
 *
 * 激活时广播 Event.QuickBar.SetSlot.N，N 即目标槽位索引，由 UAS_QuickBarInputHost
 * 收取后调用 QuickBar.SetActiveSlotIndex。本技能不直接触碰快捷栏，切换逻辑集中在宿主一处。
 *
 * 一个事件标签对应一个数字键：为 1 / 2 / 3 号键各授予一份本技能实例，
 * 各自设置不同的 SetSlotEventTag 即可，无需为每个槽位单独编写技能类。
 *
 * 蓝图默认值需设置：
 *   ActivationPolicy → OnInputTriggered（点按触发）
 *   SetSlotEventTag  → 目标槽位对应的带编号子标签
 */
class UAS_QuickBarSetSlot : UYanGameplayAbility
{
	/** 广播的切换事件，末段编号即目标槽位索引 */
	UPROPERTY(EditDefaultsOnly, Category = "QuickBar", Meta = (Categories = "Event.QuickBar.SetSlot"))
	FGameplayTag SetSlotEventTag = FGameplayTag::RequestGameplayTag(n"Event.QuickBar.SetSlot.0");

	//~Begin UGameplayAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		if (!CommitAbility())
		{
			EndAbility();
			return;
		}

		UAbilitySystemComponent ASC = GetAbilitySystemComponentFromActorInfo();
		if (ASC != nullptr)
		{
			FGameplayEventData Payload;
			ASC.SendGameplayEvent(SetSlotEventTag, Payload);
		}

		EndAbility();
	}
	//~End UGameplayAbility Interface
}
