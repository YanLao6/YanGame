/**
 * UAS_QuickBarInputHost — 快捷栏输入宿主技能。
 *
 * 常驻型技能：激活后并行挂起三个 WaitGameplayEvent 监听，把外部输入解耦为
 * 三条快捷栏操作，收到对应事件才向 Controller 上的 UQuickBarComponent 转发：
 *
 *   Event.QuickBar.SetSlot.N      → SetActiveSlotIndex(N)
 *   Event.QuickBar.CycleForward   → CycleActiveSlotForward()
 *   Event.QuickBar.CycleBackward  → CycleActiveSlotBackward()
 *
 * SetSlot 监听的是父标签且不做精确匹配，目标槽位由子标签末段直接表达，
 * 编号与槽位索引一致，同自 0 起算。新增槽位只需补一个子标签，
 * 发送方与本宿主都无需改动。
 *
 * 与发送方技能（AS_QuickBarSetSlot / CycleForward / CycleBackward / AS_ItemWheel）配对：
 * 发送方只负责广播事件，快捷栏读写集中在本宿主一处，便于统一处理网络路由
 * （SetActiveSlotIndex 为 Server Reliable RPC）。
 *
 * 监听全程常驻，故三个任务均以 bOnlyTriggerOnce = false 循环触发，技能不自行结束。
 *
 * 除转发外还维护两条切槽策略，二者共用同一份激活历史：
 *   - 空槽不切：目标槽位没有物品时按键不生效，避免玩家主动把自己切成空手；
 *   - 失去装备后回退：当前激活物品被移出快捷栏后，按激活历史由新到旧回到仍有物品的槽位。
 *
 * 历史由快捷栏的激活索引消息驱动而非按键处，故循环切换与服务器侧落位装备同样计入。
 * 回退以槽位内容消息为触发点：物品被 RemoveItemFromSlot 移除时只广播槽位内容变化，
 * 激活索引虽被清为 -1 却不广播，两端仅此一条消息可靠。
 *
 * 蓝图默认值需设置：
 *   ActivationPolicy → OnSpawn（Avatar 绑定后自动激活并持续监听）
 */
class UAS_QuickBarInputHost : UYanGameplayAbility
{
	/** 请求切换槽位的事件父标签，其下带编号的子标签均由本宿主收取 */
	UPROPERTY(EditDefaultsOnly, Category = "QuickBar", Meta = (Categories = "Event.QuickBar"))
	FGameplayTag SetSlotEventTag = FGameplayTag::RequestGameplayTag(n"Event.QuickBar.SetSlot");

	/** 请求向前循环切换的事件 */
	UPROPERTY(EditDefaultsOnly, Category = "QuickBar", Meta = (Categories = "Event.QuickBar"))
	FGameplayTag CycleForwardEventTag = FGameplayTag::RequestGameplayTag(n"Event.QuickBar.CycleForward");

	/** 请求向后循环切换的事件 */
	UPROPERTY(EditDefaultsOnly, Category = "QuickBar", Meta = (Categories = "Event.QuickBar"))
	FGameplayTag CycleBackwardEventTag = FGameplayTag::RequestGameplayTag(n"Event.QuickBar.CycleBackward");

	// 玩家激活过的槽位，末尾为最近一次；同一槽位只保留一条，故长度不超过槽位数
	private TArray<int32> ActiveSlotHistory;

	private FGameplayMessageListenerHandle SlotsListener;
	private FGameplayMessageListenerHandle ActiveIndexListener;

	//~Begin UGameplayAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		// 三条监听常驻，bOnlyTriggerOnce = false 使其循环触发；本技能不 CommitAbility、不自行结束
		StartListening(SetSlotEventTag, n"HandleSetSlot", false);
		StartListening(CycleForwardEventTag, n"HandleCycleForward", true);
		StartListening(CycleBackwardEventTag, n"HandleCycleBackward", true);

		StartWatchingQuickBar();
	}

	UFUNCTION(BlueprintOverride)
	void OnEndAbility(bool bWasCancelled)
	{
		StopWatchingQuickBar();
	}
	//~End UGameplayAbility Interface

	private void StartListening(FGameplayTag EventTag, FName HandlerName, bool bMatchExact)
	{
		UAbilityTask_WaitGameplayEvent Task = AngelscriptAbilityTask::WaitGameplayEvent(
			this, EventTag, nullptr, false, bMatchExact);
		Task.EventReceived.AddUFunction(this, HandlerName);
		Task.ReadyForActivation();
	}

	UFUNCTION()
	private void HandleSetSlot(FGameplayEventData Payload)
	{
		const int32 SlotIndex = ResolveSlotIndex(Payload.EventTag);
		if (SlotIndex < 0)
		{
			return;
		}

		UQuickBarComponent QuickBar = GetQuickBar();
		if (QuickBar == nullptr)
		{
			return;
		}

		// 空槽位不接受按键：切过去只会卸下当前装备而拿不到新的，等同于把玩家切成空手
		if (!IsSlotOccupied(QuickBar, SlotIndex))
		{
			return;
		}

		QuickBar.SetActiveSlotIndex(SlotIndex);
	}

	// 非精确匹配下父标签自身也会送达，其末段非数字，故须先判定数字再取值，否则会误切到索引 0
	private int32 ResolveSlotIndex(FGameplayTag EventTag) const
	{
		TArray<FName> Components = OUUGameplayTag::GetTagComponents(EventTag);
		if (Components.Num() == 0)
		{
			return -1;
		}

		const FString LeafComponent = Components[Components.Num() - 1].ToString();
		return LeafComponent.IsNumeric() ? String::Conv_StringToInt(LeafComponent) : -1;
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

	private void StartWatchingQuickBar()
	{
		// 技能可能被取消后重新激活，先解绑避免同一实例上叠加监听
		StopWatchingQuickBar();

		UGameplayMessageSubsystem Messaging = UGameplayMessageSubsystem::Get();
		ActiveIndexListener = Messaging.RegisterListener(
			GameplayTags::EM_QuickBar_Message_ActiveIndexChanged,
			this,
			n"HandleActiveIndexChanged",
			FQuickBarActiveIndexChangedMessage());
		SlotsListener = Messaging.RegisterListener(
			GameplayTags::EM_QuickBar_Message_SlotsChanged,
			this,
			n"HandleSlotsChanged",
			FQuickBarSlotsChangedMessage());
	}

	private void StopWatchingQuickBar()
	{
		ActiveIndexListener.Unregister();
		SlotsListener.Unregister();
	}

	// 历史记在激活索引变化处而非按键处，循环切换与服务器侧落位装备因此同样计入
	UFUNCTION()
	private void HandleActiveIndexChanged(FGameplayTag Channel, FQuickBarActiveIndexChangedMessage Payload)
	{
		if (!IsOwnQuickBar(Payload.Owner))
		{
			return;
		}

		RecordActiveSlot(Payload.ActiveIndex);
	}

	// 当前装备被移出快捷栏后回到最近一次仍有物品的槽位；槽位增删同样经此，故须先确认确实空手
	UFUNCTION()
	private void HandleSlotsChanged(FGameplayTag Channel, FQuickBarSlotsChangedMessage Payload)
	{
		if (!IsOwnQuickBar(Payload.Owner))
		{
			return;
		}

		UQuickBarComponent QuickBar = GetQuickBar();
		if (QuickBar == nullptr || QuickBar.GetActiveSlotItem() != nullptr)
		{
			return;
		}

		const int32 FallbackIndex = FindLatestOccupiedSlotInHistory(QuickBar);
		if (FallbackIndex >= 0)
		{
			QuickBar.SetActiveSlotIndex(FallbackIndex);
		}
	}

	// 同一槽位只保留最近一条，重复激活不会把历史撑长，也不会让旧记录压过新记录
	private void RecordActiveSlot(int32 SlotIndex)
	{
		if (SlotIndex < 0)
		{
			return;
		}

		for (int32 Index = 0; Index < ActiveSlotHistory.Num(); ++Index)
		{
			if (ActiveSlotHistory[Index] == SlotIndex)
			{
				ActiveSlotHistory.RemoveAt(Index);
				break;
			}
		}

		ActiveSlotHistory.Add(SlotIndex);
	}

	// 由新到旧回溯；已清空的历史槽位直接跳过，其记录留待该槽位再次装填时复用
	private int32 FindLatestOccupiedSlotInHistory(UQuickBarComponent QuickBar) const
	{
		TArray<UInventoryItemInstance> Slots = QuickBar.GetSlots();
		for (int32 Index = ActiveSlotHistory.Num() - 1; Index >= 0; --Index)
		{
			const int32 SlotIndex = ActiveSlotHistory[Index];
			if (Slots.IsValidIndex(SlotIndex) && Slots[SlotIndex] != nullptr)
			{
				return SlotIndex;
			}
		}

		return -1;
	}

	private bool IsSlotOccupied(UQuickBarComponent QuickBar, int32 SlotIndex) const
	{
		TArray<UInventoryItemInstance> Slots = QuickBar.GetSlots();
		return Slots.IsValidIndex(SlotIndex) && Slots[SlotIndex] != nullptr;
	}

	// 快捷栏消息按玩家各广播一路，只处理本 Avatar 所属的那一路
	private bool IsOwnQuickBar(AActor MessageOwner) const
	{
		UQuickBarComponent QuickBar = GetQuickBar();
		return MessageOwner != nullptr && QuickBar != nullptr && MessageOwner == QuickBar.GetOwner();
	}

	// 快捷栏挂在 Controller 上，随 Avatar 存续，每次按需惰性获取以规避控制器复制延迟
	private UQuickBarComponent GetQuickBar() const
	{
		APawn OwningPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
		APlayerController OwningController = (OwningPawn != nullptr) ? Cast<APlayerController>(OwningPawn.GetController()) : nullptr;
		return (OwningController != nullptr) ? UQuickBarComponent::Get(OwningController) : nullptr;
	}
}
