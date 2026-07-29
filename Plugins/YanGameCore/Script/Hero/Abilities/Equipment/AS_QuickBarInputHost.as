/**
 * UAS_QuickBarInputHost — 快捷栏输入宿主技能。
 *
 * 常驻型技能：激活后并行挂起三个 WaitGameplayEvent 监听，把外部输入解耦为
 * 三条快捷栏操作，收到对应事件才向 Controller 上的 UQuickBarComponent 转发：
 *
 *   Event.QuickBar.SetSlot        → SetActiveSlotIndex(EventMagnitude 截断为索引)
 *   Event.QuickBar.CycleForward   → CycleActiveSlotForward()
 *   Event.QuickBar.CycleBackward  → CycleActiveSlotBackward()
 *
 * 与三个发送方技能（AS_QuickBarSetSlot / CycleForward / CycleBackward）配对：
 * 发送方只负责广播事件，快捷栏读写集中在本宿主一处，便于统一处理网络路由
 * （SetActiveSlotIndex 为 Server Reliable RPC）。
 *
 * 监听全程常驻，故三个任务均以 bOnlyTriggerOnce = false 循环触发，技能不自行结束。
 *
 * 蓝图默认值需设置：
 *   ActivationPolicy → OnSpawn（Avatar 绑定后自动激活并持续监听）
 */
class UAS_QuickBarInputHost : UYanGameplayAbility
{
	/** 请求切换到指定索引槽位的事件，索引由 EventMagnitude 携带 */
	UPROPERTY(EditDefaultsOnly, Category = "QuickBar", Meta = (Categories = "Event.QuickBar"))
	FGameplayTag SetSlotEventTag = FGameplayTag::RequestGameplayTag(n"Event.QuickBar.SetSlot");

	/** 请求向前循环切换的事件 */
	UPROPERTY(EditDefaultsOnly, Category = "QuickBar", Meta = (Categories = "Event.QuickBar"))
	FGameplayTag CycleForwardEventTag = FGameplayTag::RequestGameplayTag(n"Event.QuickBar.CycleForward");

	/** 请求向后循环切换的事件 */
	UPROPERTY(EditDefaultsOnly, Category = "QuickBar", Meta = (Categories = "Event.QuickBar"))
	FGameplayTag CycleBackwardEventTag = FGameplayTag::RequestGameplayTag(n"Event.QuickBar.CycleBackward");

	//~Begin UGameplayAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		// 三条监听常驻，bOnlyTriggerOnce = false 使其循环触发；本技能不 CommitAbility、不自行结束
		StartListening(SetSlotEventTag, n"HandleSetSlot");
		StartListening(CycleForwardEventTag, n"HandleCycleForward");
		StartListening(CycleBackwardEventTag, n"HandleCycleBackward");
	}
	//~End UGameplayAbility Interface

	private void StartListening(FGameplayTag EventTag, FName HandlerName)
	{
		UAbilityTask_WaitGameplayEvent Task = AngelscriptAbilityTask::WaitGameplayEvent(
			this, EventTag, nullptr, false, true);
		Task.EventReceived.AddUFunction(this, HandlerName);
		Task.ReadyForActivation();
	}

	UFUNCTION()
	private void HandleSetSlot(FGameplayEventData Payload)
	{
		UQuickBarComponent QuickBar = GetQuickBar();
		if (QuickBar != nullptr)
		{
			// float→int32 强转向零截断，等价蓝图的 FTrunc
			QuickBar.SetActiveSlotIndex(int32(Payload.EventMagnitude));
		}
	}

	UFUNCTION()
	private void HandleCycleForward(FGameplayEventData Payload)
	{
		UQuickBarComponent QuickBar = GetQuickBar();
		if (QuickBar != nullptr)
		{
			QuickBar.CycleActiveSlotForward();
		}
	}

	UFUNCTION()
	private void HandleCycleBackward(FGameplayEventData Payload)
	{
		UQuickBarComponent QuickBar = GetQuickBar();
		if (QuickBar != nullptr)
		{
			QuickBar.CycleActiveSlotBackward();
		}
	}

	// 快捷栏挂在 Controller 上，随 Avatar 存续，每次按需惰性获取以规避控制器复制延迟
	private UQuickBarComponent GetQuickBar() const
	{
		APawn OwningPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
		APlayerController OwningController = (OwningPawn != nullptr) ? Cast<APlayerController>(OwningPawn.GetController()) : nullptr;
		return (OwningController != nullptr) ? UQuickBarComponent::Get(OwningController) : nullptr;
	}
}
