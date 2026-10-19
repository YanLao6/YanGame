/**
 * UYanLoadoutSelectWidget — 特长选取界面：四个候选，一次选定一个。
 *
 * 开局有一个槽位，之后每占领一次资源点多一个，直到四个候选全部选完。
 * 选定即锁死，界面因此不提供撤销：已选的按钮永久禁用并染色。
 *
 * 输入模式取 GameAndMenu 而非 Menu：本界面在对局进行中弹出，冻结操作等于把玩家
 * 白送给对手。玩家可以边跑边选，也因此能再按一次开关键关掉界面。
 *
 * 按钮不在 UMG 里摆，而是按候选表在运行时生成：候选项增减无须改动 WBP，
 * 也不必维护一串靠名字匹配的绑定——名字拼错时 BindWidget 是静默失效的。
 * 代价是按钮数量上限固定为 MaxSpecialtyButtons：AngelScript 的委托只能绑具名
 * UFUNCTION，无法按索引生成回调。
 *
 * 数据取自本机 PlayerState 上的 UYanLoadoutComponent，由服务器复制而来。
 * 该组件生命周期长于本界面，故不绑其委托而按定时器轮询——避免在其委托列表上留下死绑定。
 *
 * 点击不做本地预判：服务器可能因槽位已用尽而驳回，界面等复制回来再刷新。
 *
 * UMG 布局需绑定（变量名需一致）：
 *   UPanelWidget SpecialtyBox —— 候选按钮的容器，任意 Panel 均可；设计期应留空
 * 可选绑定：
 *   UTextBlock   StatusText   —— 剩余槽位提示
 */
class UYanLoadoutSelectWidget : UYanUserWidget
{
	// 对局中弹出，不能夺走移动权；同时保证开关键能再次抵达技能
	default InputConfig = EModularInputWidgetInputMode::GameAndMenu;
	default bIsBackHandler = true;

	/** 候选按钮容器，子控件由本控件独占管理 */
	UPROPERTY(BindWidget)
	UPanelWidget SpecialtyBox;

	UPROPERTY(BindWidgetOptional)
	UTextBlock StatusText;

	/** 尚未选定的候选按钮底色 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout")
	FLinearColor AvailableColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	/** 已选定的候选按钮底色 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout")
	FLinearColor SelectedColor = FLinearColor(0.20f, 0.70f, 1.00f, 1.0f);

	/** 选完最后一个待用槽位后自动关闭界面 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout")
	bool bCloseWhenNoSlotLeft = true;

	/** 状态轮询间隔（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout", Meta = (ClampMin = "0.05"))
	float UpdateInterval = 0.2f;

	// 生成的候选按钮上限，与下方具名点击回调的数量一致
	const int32 MaxSpecialtyButtons = 4;

	// 已生成的候选按钮，下标即候选表索引；持有引用同时兼作防 GC
	private TArray<UButton> SpecialtyButtons;

	private FTimerHandle UpdateTimer;

	UFUNCTION(BlueprintOverride)
	void Construct()
	{
		BuildSpecialtyButtons();
		Refresh();

		UpdateTimer = System::SetTimer(this, n"Refresh", UpdateInterval, true);
	}

	UFUNCTION(BlueprintOverride)
	void Destruct()
	{
		System::ClearAndInvalidateTimerHandle(UpdateTimer);
	}

	// ── 构建 ─────────────────────────────────────────────────────────────────

	// 候选表随组件类一同加载，不必等复制，故只在取到组件时生成一次
	private void BuildSpecialtyButtons()
	{
		if (SpecialtyBox == nullptr)
		{
			return;
		}

		UYanLoadoutComponent Loadout = GetLoadoutComponent();
		if (Loadout == nullptr)
		{
			return;
		}

		SpecialtyBox.ClearChildren();
		SpecialtyButtons.Empty();

		TArray<FYanSpecialtyEntry> Specialties = Loadout.GetAvailableSpecialties();
		const int32 Count = Math::Min(Specialties.Num(), MaxSpecialtyButtons);

		for (int32 Index = 0; Index < Count; Index++)
		{
			UButton Button = Cast<UButton>(NewObject(this, UButton));
			UTextBlock Label = Cast<UTextBlock>(NewObject(this, UTextBlock));
			if (Button == nullptr || Label == nullptr)
			{
				continue;
			}

			Label.SetText(Specialties[Index].DisplayName);
			Button.AddChild(Label);
			SpecialtyBox.AddChild(Button);

			SpecialtyButtons.Add(Button);
		}

		BindClickHandlers();
	}

	// 委托只能绑具名 UFUNCTION，故按索引逐个绑定而非循环
	private void BindClickHandlers()
	{
		if (SpecialtyButtons.Num() > 0)
		{
			SpecialtyButtons[0].OnClicked.AddUFunction(this, n"OnSpecialty0Clicked");
		}
		if (SpecialtyButtons.Num() > 1)
		{
			SpecialtyButtons[1].OnClicked.AddUFunction(this, n"OnSpecialty1Clicked");
		}
		if (SpecialtyButtons.Num() > 2)
		{
			SpecialtyButtons[2].OnClicked.AddUFunction(this, n"OnSpecialty2Clicked");
		}
		if (SpecialtyButtons.Num() > 3)
		{
			SpecialtyButtons[3].OnClicked.AddUFunction(this, n"OnSpecialty3Clicked");
		}
	}

	// ── 输入 ─────────────────────────────────────────────────────────────────

	UFUNCTION()
	private void OnSpecialty0Clicked()
	{
		RequestSelect(0);
	}

	UFUNCTION()
	private void OnSpecialty1Clicked()
	{
		RequestSelect(1);
	}

	UFUNCTION()
	private void OnSpecialty2Clicked()
	{
		RequestSelect(2);
	}

	UFUNCTION()
	private void OnSpecialty3Clicked()
	{
		RequestSelect(3);
	}

	private void RequestSelect(int32 Index)
	{
		UYanLoadoutComponent Loadout = GetLoadoutComponent();
		if (Loadout == nullptr || !Loadout.HasPendingSelection() || Loadout.IsSpecialtySelected(Index))
		{
			return;
		}

		Loadout.RequestSelectSpecialty(Index);

		// 本机先行关闭而不等复制回来：请求被驳回的唯一情形是槽位已被占用，
		// 那种情况下界面留着也无事可做
		if (bCloseWhenNoSlotLeft && Loadout.GetPendingSlotCount() <= 1)
		{
			DeactivateWidget();
		}
	}

	// ── 刷新 ─────────────────────────────────────────────────────────────────

	UFUNCTION()
	private void Refresh()
	{
		UYanLoadoutComponent Loadout = GetLoadoutComponent();
		if (Loadout == nullptr)
		{
			SetAllEnabled(false);
			SetStatus("正在等待对局数据…");
			return;
		}

		// 构建时组件可能尚未就绪，按钮因而一个都没生成，取到组件后补一次
		if (SpecialtyButtons.Num() == 0)
		{
			BuildSpecialtyButtons();
		}

		const bool bHasSlot = Loadout.HasPendingSelection();

		for (int32 Index = 0; Index < SpecialtyButtons.Num(); Index++)
		{
			const bool bSelected = Loadout.IsSpecialtySelected(Index);

			SpecialtyButtons[Index].SetIsEnabled(bHasSlot && !bSelected);
			SpecialtyButtons[Index].SetBackgroundColor(bSelected ? SelectedColor : AvailableColor);
		}

		RefreshStatus(Loadout);
	}

	private void RefreshStatus(UYanLoadoutComponent Loadout)
	{
		if (StatusText == nullptr)
		{
			return;
		}

		const int32 Pending = Loadout.GetPendingSlotCount();
		if (Pending > 0)
		{
			SetStatus("可选 " + String::Conv_IntToString(Pending) + " 项特长");
			return;
		}

		if (Loadout.IsFullyUnlocked())
		{
			SetStatus("特长已全部解锁");
			return;
		}

		SetStatus("占领资源点解锁下一项特长");
	}

	private void SetAllEnabled(bool bEnabled)
	{
		for (UButton Button : SpecialtyButtons)
		{
			if (Button != nullptr)
			{
				Button.SetIsEnabled(bEnabled);
			}
		}
	}

	private void SetStatus(FString Message)
	{
		if (StatusText != nullptr)
		{
			StatusText.SetText(FText::FromString(Message));
		}
	}

	// PlayerState 随连接复制到达，界面构建时未必就绪，故每次取用重新解析
	private UYanLoadoutComponent GetLoadoutComponent()
	{
		APlayerController OwningController = GetOwningPlayer();
		if (OwningController == nullptr || OwningController.PlayerState == nullptr)
		{
			return nullptr;
		}

		return UYanLoadoutComponent::Get(OwningController.PlayerState);
	}
}
