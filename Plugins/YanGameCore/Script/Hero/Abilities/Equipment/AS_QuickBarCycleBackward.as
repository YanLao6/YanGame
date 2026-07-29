/**
 * UAS_QuickBarCycleBackward — 快捷栏向后循环切换（发送方）。
 *
 * 激活时广播 Event.QuickBar.CycleBackward，由 UAS_QuickBarInputHost 收取后
 * 调用 QuickBar.CycleActiveSlotBackward，切换到上一个非空槽位。
 *
 * 蓝图默认值需设置：
 *   ActivationPolicy → OnInputTriggered（点按触发，如滚轮向后）
 */
class UAS_QuickBarCycleBackward : UYanGameplayAbility
{
	/** 广播的切换事件，须与宿主监听的 CycleBackward 事件一致 */
	UPROPERTY(EditDefaultsOnly, Category = "QuickBar", Meta = (Categories = "Event.QuickBar"))
	FGameplayTag CycleBackwardEventTag = FGameplayTag::RequestGameplayTag(n"Event.QuickBar.CycleBackward");

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
			ASC.SendGameplayEvent(CycleBackwardEventTag, Payload);
		}

		EndAbility();
	}
	//~End UGameplayAbility Interface
}
