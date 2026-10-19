/**
 * UYanLoadoutPromptWidget — 资源点占领读条与「有特长可选」提示。
 *
 * 常驻 HUD，但只在有话可说时可见：读条中显示进度，解锁后转为提示按键。
 * 无事发生时整体折叠，不占据版面。
 *
 * 占领是一段长达数秒、玩家站着不动的过程，没有读条就无从判断自己是否真在占领。
 * 而解锁本身发生在玩家可能正被追打的时刻，故不弹窗打断，只在 HUD 上留一条提示，
 * 由玩家自己挑安全时机按键去选。
 *
 * 数据取自本机 PlayerState 上的 UYanLoadoutComponent，按定时器轮询——
 * 该组件生命周期长于本界面，绑其委托会在其委托列表上留下死绑定。
 *
 * UMG 布局需绑定（变量名需一致）：
 *   UTextBlock   PromptText —— 提示文案
 * 可选绑定：
 *   UProgressBar CaptureBar —— 占领进度条
 */
class UYanLoadoutPromptWidget : UYanUserWidget
{
	UPROPERTY(BindWidget)
	UTextBlock PromptText;

	UPROPERTY(BindWidgetOptional)
	UProgressBar CaptureBar;

	/** 有待选槽位时的提示文案，其中的按键名须与实际绑定一致 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout")
	FText PendingPrompt;
	default PendingPrompt = FText::FromString("按 Tab 选择新特长");

	/** 占领读条中的提示文案 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout")
	FText CapturingPrompt;
	default CapturingPrompt = FText::FromString("占领中…");

	/** 状态轮询间隔（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout", Meta = (ClampMin = "0.05"))
	float UpdateInterval = 0.1f;

	private FTimerHandle UpdateTimer;

	UFUNCTION(BlueprintOverride)
	void Construct()
	{
		Refresh();
		UpdateTimer = System::SetTimer(this, n"Refresh", UpdateInterval, true);
	}

	UFUNCTION(BlueprintOverride)
	void Destruct()
	{
		System::ClearAndInvalidateTimerHandle(UpdateTimer);
	}

	UFUNCTION()
	private void Refresh()
	{
		UYanLoadoutComponent Loadout = GetLoadoutComponent();
		if (Loadout == nullptr)
		{
			SetVisibility(ESlateVisibility::Collapsed);
			return;
		}

		// 待选优先于读条：已经能选了，进度涨到哪一格就无关紧要了
		if (Loadout.HasPendingSelection())
		{
			SetVisibility(ESlateVisibility::HitTestInvisible);
			PromptText.SetText(PendingPrompt);
			SetCaptureFraction(1.0f);
			return;
		}

		const float Progress = Loadout.GetCaptureProgress();
		if (Progress <= 0.0f)
		{
			SetVisibility(ESlateVisibility::Collapsed);
			return;
		}

		SetVisibility(ESlateVisibility::HitTestInvisible);
		PromptText.SetText(CapturingPrompt);
		SetCaptureFraction(Progress);
	}

	private void SetCaptureFraction(float Fraction)
	{
		if (CaptureBar != nullptr)
		{
			CaptureBar.SetPercent(Math::Clamp(Fraction, 0.0f, 1.0f));
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
