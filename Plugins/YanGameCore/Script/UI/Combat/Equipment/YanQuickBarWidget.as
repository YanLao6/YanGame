/**
 * YanQuickBarWidget — 快捷栏（一组槽位）。
 *
 * 定位本地玩家 Controller 上的 UQuickBarComponent（该组件挂在 Controller 上），
 * 把 SlotIndices 指定的槽位实例化成 YanQuickBarSlotWidget 子控件填入绑定容器，
 * 并高亮当前激活槽位。
 *
 * 同一个快捷栏组件可由多个本控件分头呈现：各自配置不同的 SlotIndices，
 * 即可把同一批槽位拆到屏幕各处。因此"呈现位置"与"快捷栏槽位索引"是两套下标，
 * 按键提示与激活高亮均按后者判定。
 *
 * 数据由快捷栏的槽位内容与激活索引两条消息驱动，服务器与客户端两侧均会广播。
 * 消息只在状态变化时到达，以下两种情形不会补发，故另有低频重试兜底：
 *   - 控件构造时快捷栏组件尚未复制到位；
 *   - 物品图标仍在 DataRegistry 异步流送。
 * 重试在状态补齐后自行停止，稳定态下不产生轮询开销。
 *
 * UMG 布局需绑定（变量名需一致）：
 *   UPanelWidget SlotContainer —— 槽位容器（HorizontalBox / UniformGrid 等任意 Panel）
 * WBP 默认值需设置：
 *   SlotWidgetClass → 你的 YanQuickBarSlotWidget 蓝图子类
 *   SlotIndices     → 本控件呈现的槽位；留空则呈现全部
 */
class UYanQuickBarWidget : UYanUserWidget
{
	/** 槽位容器，槽位子控件运行时填入此处 */
	UPROPERTY(BindWidget)
	UPanelWidget SlotContainer;

	/** 槽位子控件类，需为 UYanQuickBarSlotWidget 的蓝图子类 */
	UPROPERTY(EditDefaultsOnly, Category = "QuickBar")
	TSubclassOf<UYanQuickBarSlotWidget> SlotWidgetClass;

	/**
	 * 本控件呈现的快捷栏槽位索引，子控件按此处的先后顺序排布。
	 * 留空表示呈现全部槽位；超出快捷栏槽位数的索引直接跳过，不占位。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "QuickBar")
	TArray<int32> SlotIndices;

	/** 组件未到位或图标未就绪时的重试间隔（秒），补齐后自动停止 */
	UPROPERTY(EditDefaultsOnly, Category = "QuickBar", Meta = (ClampMin = "0.05"))
	float SyncRetryInterval = 0.25f;

	// QuickBar 挂在 Controller 上，且 Controller 复制有延迟，故惰性获取
	private UQuickBarComponent CachedQuickBar;
	// 已生成的槽位子控件，按槽位索引对应
	private TArray<UW_ItemSlot> SlotWidgets;

	private FGameplayMessageListenerHandle SlotsListener;
	private FGameplayMessageListenerHandle ActiveIndexListener;

	// 两条消息各带一半状态，合并后才能完整呈现
	private TArray<UInventoryItemInstance> CachedSlots;
	private int32 CachedActiveIndex = -1;

	// 由 SlotIndices 从缓存状态解析出的呈现内容，三者下标一致，均以呈现位为序
	private TArray<UInventoryItemInstance> VisibleItems;
	private TArray<int32> VisibleSlotIndices;
	private int32 VisibleActiveIndex = -1;

	private FTimerHandle RetryTimer;
	private bool bRetryActive = false;

	UFUNCTION(BlueprintOverride)
	void Construct()
	{
		UGameplayMessageSubsystem Messaging = UGameplayMessageSubsystem::Get();
		SlotsListener = Messaging.RegisterListener(
			GameplayTags::EM_QuickBar_Message_SlotsChanged,
			this,
			n"HandleSlotsChanged",
			FQuickBarSlotsChangedMessage());
		ActiveIndexListener = Messaging.RegisterListener(
			GameplayTags::EM_QuickBar_Message_ActiveIndexChanged,
			this,
			n"HandleActiveIndexChanged",
			FQuickBarActiveIndexChangedMessage());

		PullFromComponent();
	}

	UFUNCTION(BlueprintOverride)
	void Destruct()
	{
		SlotsListener.Unregister();
		ActiveIndexListener.Unregister();
		StopRetry();
	}

	UFUNCTION()
	private void HandleSlotsChanged(FGameplayTag Channel, FQuickBarSlotsChangedMessage Payload)
	{
		if (!IsLocalPlayerOwner(Payload.Owner))
		{
			return;
		}

		// 消息里的槽位元素是 TObjectPtr，逐个取出以对齐句柄类型
		CachedSlots.Empty();
		for (int32 Index = 0; Index < Payload.Slots.Num(); Index++)
		{
			CachedSlots.Add(Payload.Slots[Index]);
		}
		RefreshQuickBar();
	}

	UFUNCTION()
	private void HandleActiveIndexChanged(FGameplayTag Channel, FQuickBarActiveIndexChangedMessage Payload)
	{
		if (!IsLocalPlayerOwner(Payload.Owner))
		{
			return;
		}

		CachedActiveIndex = Payload.ActiveIndex;
		RefreshQuickBar();
	}

	// 组件延迟到位与图标异步流送的兜底：直接向组件拉取当前状态
	UFUNCTION()
	private void PullFromComponent()
	{
		UQuickBarComponent QuickBar = GetQuickBar();
		if (QuickBar == nullptr)
		{
			StartRetry();
			return;
		}

		CachedSlots = QuickBar.GetSlots();
		CachedActiveIndex = QuickBar.GetActiveSlotIndex();
		RefreshQuickBar();
	}

	private void RefreshQuickBar()
	{
		if (SlotContainer == nullptr || SlotWidgetClass.Get() == nullptr)
		{
			return;
		}

		CollectVisibleSlots();

		// 槽位尚未复制到位时呈现列表必为空，空列表在 Sync 看来即已就绪，故另行确认
		const bool bReady = EquipmentSlotView::Sync(
			this, SlotContainer, SlotWidgetClass, SlotWidgets, VisibleItems, VisibleActiveIndex, VisibleSlotIndices)
			&& CachedSlots.Num() > 0;

		if (bReady)
		{
			StopRetry();
		}
		else
		{
			StartRetry();
		}
	}

	// 把快捷栏槽位索引映射为呈现位：VisibleSlotIndices[i] 即呈现位 i 所对应的槽位
	private void CollectVisibleSlots()
	{
		VisibleItems.Reset();
		VisibleSlotIndices.Reset();
		VisibleActiveIndex = -1;

		const bool bUseAllSlots = SlotIndices.Num() == 0;
		const int32 Count = bUseAllSlots ? CachedSlots.Num() : SlotIndices.Num();
		for (int32 Index = 0; Index < Count; Index++)
		{
			const int32 SlotIndex = bUseAllSlots ? Index : SlotIndices[Index];
			if (!CachedSlots.IsValidIndex(SlotIndex))
			{
				continue;
			}

			if (SlotIndex == CachedActiveIndex)
			{
				VisibleActiveIndex = VisibleItems.Num();
			}
			VisibleItems.Add(CachedSlots[SlotIndex]);
			VisibleSlotIndices.Add(SlotIndex);
		}
	}

	// 广播来自各玩家的快捷栏，只响应本地玩家的那一路
	private bool IsLocalPlayerOwner(AActor MessageOwner) const
	{
		AActor OwningActor = GetOwningPlayer();
		return MessageOwner != nullptr && MessageOwner == OwningActor;
	}

	private void StartRetry()
	{
		if (!bRetryActive)
		{
			bRetryActive = true;
			RetryTimer = System::SetTimer(this, n"PullFromComponent", SyncRetryInterval, true);
		}
	}

	private void StopRetry()
	{
		if (bRetryActive)
		{
			bRetryActive = false;
			System::ClearAndInvalidateTimerHandle(RetryTimer);
		}
	}

	private UQuickBarComponent GetQuickBar()
	{
		if (CachedQuickBar == nullptr)
		{
			APlayerController OwningController = GetOwningPlayer();
			if (OwningController != nullptr)
			{
				CachedQuickBar = UQuickBarComponent::Get(OwningController);
			}
		}
		return CachedQuickBar;
	}
}
