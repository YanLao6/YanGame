/**
 * YanHeroSelectWidget — 大厅角色选择界面。
 *
 * 激活期间使用 Menu 输入模式，游戏侧输入被完全屏蔽，直到全员确认后切图。
 *
 * 界面只负责发起请求与呈现结果，一切裁决在服务器：
 *   选人   → UHeroSelectionComponent::RequestSelectHeroByIndex（Server RPC）
 *   确认   → UHeroSelectionComponent::RequestSetReady（Server RPC）
 *   切图   → 服务器在人数达标且全员确认后自行发起，客户端被一并带走
 *
 * 全场进度来自 GameState 上的 UHeroSelectionBoardComponent，它复制给所有客户端，
 * 因而队友选走的角色在本机同样呈现为不可选。看板经 GameState 复制到达，
 * 界面构建时未必就绪，故惰性绑定并在其到达前按定时器重试。
 *
 * UMG 布局需绑定（变量名需一致）：
 *   UButton    Hero0Button      —— 选择目录中第 0 个角色
 *   UButton    Hero1Button      —— 选择目录中第 1 个角色
 *   UButton    ConfirmButton    —— 确认 / 取消确认
 * 可选绑定：
 *   UTextBlock Hero0NameText、Hero1NameText —— 角色名，取自 HeroCatalog
 *   UTextBlock ConfirmButtonText            —— 确认按钮文案
 *   UTextBlock StatusText                   —— 当前进度提示
 *
 * 角色目录配置在大厅 Experience 注入的 UHeroSelectionComponent 上，
 * 对局 Experience 的 UHeroAssignmentComponent 须引用同一份目录资产。
 */
class UYanHeroSelectWidget : UYanUserWidget
{
	// 选人期间不接受游戏输入，避免玩家在大厅操作角色
	default InputConfig = EModularInputWidgetInputMode::Menu;

	UPROPERTY(BindWidget)
	UButton Hero0Button;

	UPROPERTY(BindWidget)
	UButton Hero1Button;

	UPROPERTY(BindWidget)
	UButton ConfirmButton;

	UPROPERTY(BindWidgetOptional)
	UTextBlock Hero0NameText;

	UPROPERTY(BindWidgetOptional)
	UTextBlock Hero1NameText;

	UPROPERTY(BindWidgetOptional)
	UTextBlock ConfirmButtonText;

	UPROPERTY(BindWidgetOptional)
	UTextBlock StatusText;

	/** 确认按钮在两种状态下的文案 */
	UPROPERTY(EditDefaultsOnly, Category = "HeroSelect")
	FText ConfirmLabel;
	default ConfirmLabel = FText::FromString("确认");

	UPROPERTY(EditDefaultsOnly, Category = "HeroSelect")
	FText CancelReadyLabel;
	default CancelReadyLabel = FText::FromString("取消确认");

	/** 看板尚未复制到本机时的重试间隔（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "HeroSelect", Meta = (ClampMin = "0.05"))
	float BoardRetryInterval = 0.25f;

	private UHeroSelectionComponent SelectionComponent;
	private UHeroSelectionBoardComponent SelectionBoard;

	private FTimerHandle RetryTimer;
	private bool bRetryActive = false;

	UFUNCTION(BlueprintOverride)
	void Construct()
	{
		Hero0Button.OnClicked.AddUFunction(this, n"OnHero0Clicked");
		Hero1Button.OnClicked.AddUFunction(this, n"OnHero1Clicked");
		ConfirmButton.OnClicked.AddUFunction(this, n"OnConfirmClicked");

		RefreshHeroNames();

		if (!TryBindBoard())
		{
			StartRetry();
		}
		Refresh();
	}

	UFUNCTION(BlueprintOverride)
	void Destruct()
	{
		StopRetry();
	}

	// ── 输入 ─────────────────────────────────────────────────────────────────

	UFUNCTION()
	private void OnHero0Clicked()
	{
		RequestSelect(0);
	}

	UFUNCTION()
	private void OnHero1Clicked()
	{
		RequestSelect(1);
	}

	UFUNCTION()
	private void OnConfirmClicked()
	{
		UHeroSelectionComponent Selection = GetSelectionComponent();
		if (Selection == nullptr)
		{
			return;
		}

		Selection.RequestSetReady(!Selection.IsReady());
	}

	// 请求不做本地预判：服务器可能因角色被抢先选走而拒绝，界面等看板回来再刷新
	private void RequestSelect(int32 HeroIndex)
	{
		UHeroSelectionComponent Selection = GetSelectionComponent();
		if (Selection == nullptr)
		{
			return;
		}

		Selection.RequestSelectHeroByIndex(HeroIndex);
	}

	// ── 刷新 ─────────────────────────────────────────────────────────────────

	UFUNCTION()
	private void OnBoardChanged()
	{
		Refresh();
	}

	UFUNCTION()
	private void PollBoard()
	{
		if (TryBindBoard())
		{
			StopRetry();
		}

		Refresh();
	}

	private void Refresh()
	{
		UHeroSelectionComponent Selection = GetSelectionComponent();
		if (Selection == nullptr)
		{
			SetAllEnabled(false);
			SetStatus("正在等待大厅数据…");
			return;
		}

		const bool bReady = Selection.IsReady();
		const int32 SelectedIndex = Selection.GetSelectedHeroIndex();

		// 确认后不允许改选，取消确认即可重选
		Hero0Button.SetIsEnabled(!bReady && !Selection.IsHeroIndexTakenByOther(0));
		Hero1Button.SetIsEnabled(!bReady && !Selection.IsHeroIndexTakenByOther(1));
		ConfirmButton.SetIsEnabled(SelectedIndex >= 0);

		if (ConfirmButtonText != nullptr)
		{
			if (bReady)
			{
				ConfirmButtonText.SetText(CancelReadyLabel);
			}
			else
			{
				ConfirmButtonText.SetText(ConfirmLabel);
			}
		}

		RefreshStatus(bReady, SelectedIndex);
	}

	private void RefreshStatus(bool bReady, int32 SelectedIndex)
	{
		if (StatusText == nullptr)
		{
			return;
		}

		if (SelectionBoard == nullptr)
		{
			SetStatus("正在等待大厅数据…");
			return;
		}

		const int32 ActiveCount = SelectionBoard.GetActivePlayerCount();
		const int32 RequiredCount = SelectionBoard.GetRequiredPlayerCount();
		if (ActiveCount < RequiredCount)
		{
			SetStatus("等待其他玩家加入 " + FormatProgress(ActiveCount, RequiredCount));
			return;
		}

		if (!bReady)
		{
			if (SelectedIndex >= 0)
			{
				SetStatus("点击确认进入对局");
			}
			else
			{
				SetStatus("请选择一名角色");
			}
			return;
		}

		SetStatus("等待其他玩家确认 " + FormatProgress(SelectionBoard.GetReadyPlayerCount(), ActiveCount));
	}

	private void RefreshHeroNames()
	{
		UHeroSelectionComponent Selection = GetSelectionComponent();
		if (Selection == nullptr)
		{
			return;
		}

		const UHeroCatalog Catalog = Selection.GetHeroCatalog();
		if (Catalog == nullptr)
		{
			return;
		}

		if (Hero0NameText != nullptr && Catalog.Heroes.Num() > 0)
		{
			Hero0NameText.SetText(Catalog.Heroes[0].DisplayName);
		}

		if (Hero1NameText != nullptr && Catalog.Heroes.Num() > 1)
		{
			Hero1NameText.SetText(Catalog.Heroes[1].DisplayName);
		}
	}

	private void SetAllEnabled(bool bEnabled)
	{
		Hero0Button.SetIsEnabled(bEnabled);
		Hero1Button.SetIsEnabled(bEnabled);
		ConfirmButton.SetIsEnabled(bEnabled);
	}

	private FString FormatProgress(int32 Current, int32 Total) const
	{
		return String::Conv_IntToString(Current) + "/" + String::Conv_IntToString(Total);
	}

	private void SetStatus(FString Message)
	{
		if (StatusText != nullptr)
		{
			StatusText.SetText(FText::FromString(Message));
		}
	}

	// ── 惰性获取 ─────────────────────────────────────────────────────────────

	// 看板随 GameState 复制到达，界面构建时可能还没有，取到后才绑定刷新回调
	private bool TryBindBoard()
	{
		if (SelectionBoard != nullptr)
		{
			return true;
		}

		UHeroSelectionComponent Selection = GetSelectionComponent();
		if (Selection == nullptr)
		{
			return false;
		}

		SelectionBoard = Selection.GetSelectionBoard();
		if (SelectionBoard == nullptr)
		{
			return false;
		}

		SelectionBoard.OnBoardChanged.AddUFunction(this, n"OnBoardChanged");
		RefreshHeroNames();

		return true;
	}

	private UHeroSelectionComponent GetSelectionComponent()
	{
		if (SelectionComponent == nullptr)
		{
			APlayerController OwningController = GetOwningPlayer();
			if (OwningController != nullptr)
			{
				SelectionComponent = UHeroSelectionComponent::Get(OwningController);
			}
		}

		return SelectionComponent;
	}

	private void StartRetry()
	{
		if (!bRetryActive)
		{
			bRetryActive = true;
			RetryTimer = System::SetTimer(this, n"PollBoard", BoardRetryInterval, true);
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
}
