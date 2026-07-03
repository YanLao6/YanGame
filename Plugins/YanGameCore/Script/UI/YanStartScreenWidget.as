/**
 * YanStartScreenWidget — Press Start 界面（Press Start Screen）
 *
 * 用途：
 * - 作为 `UYanFrontendStateComponent::PressStartScreenClass` 对应的 UI。
 * - 玩家点击按钮或按下 SpaceBar / Enter / 手柄按键后触发 CommonUser 初始化，
 *   随后由上层 Flow 切换到 MainScreen。
 *
 * UMG 布局需绑定（变量名需一致）：
 *   - UButton PressStartButton
 *
 * 按键触发说明：
 *   Widget 激活时即调用 ListenForLoginKeyInput，无需先点鼠标，
 *   SpaceBar / Enter / 手柄按键均可直接触发登录流程。
 */
class UYanStartScreenWidget : UYanUserWidget
{
	/** 主要触发键：按下其中任意一个即触发登录 */
	UPROPERTY()
	TArray<FKey> PrimaryLoggingKeys;
	default PrimaryLoggingKeys.Add(FKey(n"SpaceBar"));
	default PrimaryLoggingKeys.Add(FKey(n"Enter"));
	default PrimaryLoggingKeys.Add(FKey(n"GamepadSpecialRight"));
	default PrimaryLoggingKeys.Add(FKey(n"GamepadFaceButton"));

	/** 所需修饰键列表（为空表示无需任何修饰键） */
	UPROPERTY()
	TArray<FKey> EmptyLoggingKeys;

	/** Press Start 按钮：供鼠标玩家点击，与按键路径等效 */
	UPROPERTY(BindWidget)
	UButton PressStartButton;

	UPROPERTY()
	UCommonUserSubsystem CUS;

	// 防止 OnUserInitializeComplete 被多次触发时重复调用 DeactivateWidget
	private bool bClosedForStart = false;

	UFUNCTION(BlueprintOverride)
	void PreConstruct(bool IsDesignTime)
	{
		if (IsDesignTime)
		{
			return;
		}

		CUS = Cast<UCommonUserSubsystem>(Subsystem::GetGameInstanceSubsystem(UCommonUserSubsystem));
		if (CUS != nullptr)
		{
			CUS.OnUserInitializeComplete.AddUFunction(this, n"OnUserInitializeComplete");

			// Widget 激活后立即开始监听按键。
			// 玩家无需先点鼠标——直接按 SpaceBar / Enter / 手柄键即可触发登录。
			SetListenForLogginInput(true);
		}

		if (PressStartButton != nullptr)
		{
			PressStartButton.OnClicked.AddUFunction(this, n"OnPressStartButtonClicked");
		}
	}

	/**
	 * 鼠标点击按钮触发登录。
	 * 与按键路径（ListenForLoginKeyInput）等效，
	 * 两条路径最终均通过 OnUserInitializeComplete 回调处理结果。
	 */
	UFUNCTION()
	void OnPressStartButtonClicked()
	{
		if (CUS == nullptr)
		{
			return;
		}

		PressStartButton.SetIsEnabled(false);

		if (!CUS.TryToInitializeForLocalPlay(0, FInputDeviceId(), false))
		{
			// 同步失败（不会开始异步流程），立即恢复按钮
			PressStartButton.SetIsEnabled(true);
		}
		// 异步流程已启动，等待 OnUserInitializeComplete 回调
	}

	/**
	 * CommonUser 初始化完成回调。
	 * 无论登录由按键还是鼠标按钮触发，均由此处统一处理。
	 */
	UFUNCTION()
	private void OnUserInitializeComplete(UCommonUserInfo UserInfo, bool bSuccess, FText Error, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext OnlineContext)
	{
		// 过滤非本地 0 号玩家的回调
		if (UserInfo == nullptr || UserInfo.LocalPlayerIndex != 0)
		{
			return;
		}

		if (bSuccess)
		{
			if (bClosedForStart)
			{
				return;
			}
			bClosedForStart = true;

			// 停止按键监听，关闭本界面，FrontendStateComponent ControlFlow 继续
			SetListenForLogginInput(false);
			DeactivateWidget();
			return;
		}

		// 登录失败：恢复按钮和按键监听，允许重试
		if (PressStartButton != nullptr)
		{
			PressStartButton.SetIsEnabled(true);
		}
		// 按键监听仍处于激活状态，SpaceBar 仍可重试
	}

	/** 切换 CommonUser 按键登录监听状态 */
	protected void SetListenForLogginInput(bool bShouldListen)
	{
		if (bShouldListen)
		{
			CUS.ListenForLoginKeyInput(PrimaryLoggingKeys, EmptyLoggingKeys, FCommonUserInitializeParams());
		}
		else
		{
			CUS.ListenForLoginKeyInput(EmptyLoggingKeys, EmptyLoggingKeys, FCommonUserInitializeParams());
		}
	}
}
