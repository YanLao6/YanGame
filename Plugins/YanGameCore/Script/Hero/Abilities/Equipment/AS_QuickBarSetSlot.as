/**
 * UAS_QuickBarSetSlot — 切换到指定快捷栏槽位（发送方）。
 *
 * 激活时广播 Event.QuickBar.SetSlot，并把目标槽位索引写入 EventMagnitude，
 * 由 UAS_QuickBarInputHost 收取后调用 QuickBar.SetActiveSlotIndex。
 * 本技能不直接触碰快捷栏，切换逻辑集中在宿主一处。
 *
 * 一个 SlotIndex 对应一个数字键：为 1 / 2 / 3 号键各授予一份本技能实例，
 * 各自设置不同的 SlotIndex 即可，无需为每个槽位单独编写技能类。
 *
 * 蓝图默认值需设置：
 *   ActivationPolicy → OnInputTriggered（点按触发）
 *   SlotIndex        → 目标槽位索引
 */
class UAS_QuickBarSetSlot : UYanGameplayAbility
{
	/** 目标快捷栏槽位索引，经事件 EventMagnitude 传给宿主 */
	UPROPERTY(EditDefaultsOnly, Category = "QuickBar", Meta = (ClampMin = "0"))
	int32 SlotIndex = 0;

	/** 广播的切换事件，须与宿主监听的 SetSlot 事件一致 */
	UPROPERTY(EditDefaultsOnly, Category = "QuickBar", Meta = (Categories = "Event.QuickBar"))
	FGameplayTag SetSlotEventTag = FGameplayTag::RequestGameplayTag(n"Event.QuickBar.SetSlot");

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
			Payload.EventMagnitude = float(SlotIndex);
			ASC.SendGameplayEvent(SetSlotEventTag, Payload);
		}

		EndAbility();
	}
	//~End UGameplayAbility Interface
}
