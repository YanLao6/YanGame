/**
 * UAS_ItemWheel — 物品轮盘输入宿主。
 *
 * 本身不产生战斗效果，只把一次按键分流成"装备"与"先选再装备"：
 *
 *   按下 ─┬─ HoldThreshold 内松开 → 激活本轮盘对应的快捷栏槽位
 *         └─ 超过 HoldThreshold  → 打开轮盘，松开时把轮盘选中项落入快捷栏
 *
 * 与术式轮盘的关键差别在于长按路径的终点：改选只把物品替换进快捷栏槽位，
 * 并不顺带装备，装备留给随后的一次短按。一次长按改选、多次短按取用。
 *
 * 轮盘与槽位由 SetSlotEventTag 绑定：该槽位的准入类别在 UQuickBarComponent 的
 * SlotRules 上配置，轮盘收录的物品类别须与之一致，长按落位才会落回同一槽位。
 * 短按只广播切换事件，不读槽内物品，因此不受落位后的槽位复制延迟影响；
 * 槽位为空时等同于卸下当前装备。
 *
 * 短按不直接触碰快捷栏，切换由 UAS_QuickBarInputHost 按事件标签统一执行；
 * 落位为服务器权威操作，经 UYanQuickBarRequestComponent 转交服务器，该组件需挂在 Pawn 上。
 *
 * 轮盘为 All + NoCapture 输入模式，游戏侧通路不断，按键释放仍由本技能接收，
 * 因此轮盘无需参与开合判定。
 *
 * 蓝图默认值需设置：
 *   WheelWidgetClass → 你的 YanItemWheelWidget 蓝图子类
 *   SetSlotEventTag  → 该类物品所属快捷栏槽位对应的带编号子标签
 */
class UAS_ItemWheel : UYanGameplayAbility
{
	/** 轮盘 UMG 类，需继承 UYanItemWheelWidget */
	UPROPERTY(EditDefaultsOnly, Category = "Wheel")
	TSubclassOf<UYanItemWheelWidget> WheelWidgetClass;

	/** 区分点按与长按的时长阈值（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Wheel")
	float HoldThreshold = 0.1f;

	/** 轮盘压入的 UI 层 */
	UPROPERTY(EditDefaultsOnly, Category = "Wheel", Meta = (Categories = "UI.Layer"))
	FGameplayTag WheelLayerTag = FGameplayTag::RequestGameplayTag(n"UI.Layer.Game");

	/** 短按广播的切换事件，末段编号即本轮盘绑定的槽位索引 */
	UPROPERTY(EditDefaultsOnly, Category = "Wheel", Meta = (Categories = "Event.QuickBar.SetSlot"))
	FGameplayTag SetSlotEventTag = FGameplayTag::RequestGameplayTag(n"Event.QuickBar.SetSlot.3");

	private FTimerHandle HoldTimer;
	private UYanItemWheelWidget ActiveWheel;

	//~Begin UGameplayAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		if (!CommitAbility())
		{
			EndAbility();
			return;
		}

		ActiveWheel = nullptr;

		// 仅本地客户端需要轮盘；专用服务器上等待输入释放即可
		if (IsLocallyControlled())
		{
			HoldTimer = System::SetTimer(this, n"OpenWheel", HoldThreshold, false);
		}
	}

	UFUNCTION(BlueprintOverride)
	void InputReleased()
	{
		System::ClearAndInvalidateTimerHandle(HoldTimer);

		if (ActiveWheel != nullptr)
		{
			UInventoryItemInstance PickedItem = ActiveWheel.GetSelectedItem();
			UCommonUIExtensions::PopContentFromLayer(ActiveWheel);
			ActiveWheel = nullptr;

			// 长按只改选并落位，装备交给下一次短按
			RequestPlaceInQuickBar(PickedItem);
		}
		else
		{
			ActivateQuickBarSlot();
		}

		EndAbility();
	}
	//~End UGameplayAbility Interface

	// 轮盘的选项来自背包，需先压入图层才能取到 OwningPlayer，故空轮盘于此处撤下
	UFUNCTION()
	private void OpenWheel()
	{
		APlayerController PlayerController = GetOwningPlayerController();
		if (PlayerController == nullptr || WheelWidgetClass.Get() == nullptr)
		{
			return;
		}

		UCommonActivatableWidget Pushed = UCommonUIExtensions::PushContentToLayer_ForPlayer(
			PlayerController.GetLocalPlayer(), WheelLayerTag, WheelWidgetClass);

		UYanItemWheelWidget Wheel = Cast<UYanItemWheelWidget>(Pushed);
		if (Wheel == nullptr)
		{
			return;
		}

		if (Wheel.SetupItemWheel() == 0)
		{
			UCommonUIExtensions::PopContentFromLayer(Wheel);
			return;
		}

		ActiveWheel = Wheel;
	}

	// 事件在本地 ASC 上广播，由同侧常驻的 UAS_QuickBarInputHost 收取并转交服务器
	private void ActivateQuickBarSlot()
	{
		UAbilitySystemComponent ASC = GetAbilitySystemComponentFromActorInfo();
		if (ASC != nullptr && SetSlotEventTag.IsValid())
		{
			FGameplayEventData Payload;
			ASC.SendGameplayEvent(SetSlotEventTag, Payload);
		}
	}

	private void RequestPlaceInQuickBar(UInventoryItemInstance Item)
	{
		UYanQuickBarRequestComponent Request = UYanQuickBarRequestComponent::Get(GetAvatarActorFromActorInfo());
		if (Request != nullptr && Item != nullptr)
		{
			Request.ServerPlaceItemInQuickBar(Item);
		}
	}

	private APlayerController GetOwningPlayerController() const
	{
		APawn OwningPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
		return OwningPawn != nullptr ? Cast<APlayerController>(OwningPawn.GetController()) : nullptr;
	}
}
