/**
 * UAS_QuickBarCycleForward — 快捷栏向前循环切换（发送方）。
 *
 * 激活时广播 Event.QuickBar.CycleForward，由 UAS_QuickBarInputHost 收取后
 * 调用 QuickBar.CycleActiveSlotForward，切换到下一个非空槽位。
 *
 * 蓝图默认值需设置：
 *   ActivationPolicy → OnInputTriggered（点按触发，如滚轮向前）
 */
class UAS_QuickBarCycleForward : UYanGameplayAbility
{
	/** 广播的切换事件，须与宿主监听的 CycleForward 事件一致 */
	UPROPERTY(EditDefaultsOnly, Category = "QuickBar", Meta = (Categories = "Event.QuickBar"))
	FGameplayTag CycleForwardEventTag = FGameplayTag::RequestGameplayTag(n"Event.QuickBar.CycleForward");

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
			ASC.SendGameplayEvent(CycleForwardEventTag, Payload);
		}

		EndAbility();
	}
	//~End UGameplayAbility Interface
}
