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
 * 轮盘与槽位由 QuickBarSlotIndex 绑定：该槽位的准入类别在 UQuickBarComponent 的
 * SlotRules 上配置，轮盘收录的物品类别须与之一致，长按落位才会落回同一槽位。
 * 短按只按索引激活，不读槽内物品，因此不受落位后的槽位复制延迟影响；
 * 槽位为空时等同于卸下当前装备。
 *
 * 落位为服务器权威操作，经 UYanQuickBarRequestComponent 转交服务器；该组件需挂在 Pawn 上。
 *
 * 轮盘为 All + NoCapture 输入模式，游戏侧通路不断，按键释放仍由本技能接收，
 * 因此轮盘无需参与开合判定。
 *
 * 蓝图默认值需设置：
 *   WheelWidgetClass  → 你的 YanItemWheelWidget 蓝图子类
 *   QuickBarSlotIndex → 该类物品所属的快捷栏槽位索引
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

	/** 本轮盘绑定的快捷栏槽位索引，短按激活该槽位 */
	UPROPERTY(EditDefaultsOnly, Category = "Wheel", Meta = (ClampMin = "0"))
	int32 QuickBarSlotIndex = 3;

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

	// SetActiveSlotIndex 为 Server Reliable RPC，客户端持有 Controller 所有权，可直接调用
	private void ActivateQuickBarSlot()
	{
		UQuickBarComponent QuickBar = GetQuickBar();
		if (QuickBar != nullptr)
		{
			QuickBar.SetActiveSlotIndex(QuickBarSlotIndex);
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

	// 快捷栏挂在 Controller 上，且控制器复制有延迟，故每次按需惰性获取
	private UQuickBarComponent GetQuickBar() const
	{
		APlayerController PlayerController = GetOwningPlayerController();
		return PlayerController != nullptr ? UQuickBarComponent::Get(PlayerController) : nullptr;
	}

	private APlayerController GetOwningPlayerController() const
	{
		APawn OwningPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
		return OwningPawn != nullptr ? Cast<APlayerController>(OwningPawn.GetController()) : nullptr;
	}
}
