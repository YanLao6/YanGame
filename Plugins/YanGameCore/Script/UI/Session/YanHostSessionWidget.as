/**
 * YanHostSessionWidget — 创建 Steam 联机房间
 *
 * 通过 UYanSessionManager 发起 HostSession，
 * 底层使用 UModularUserFacingExperienceDefinition::CreateHostingRequest()。
 *
 * 作为独立的 CommonUI 栈页面使用：
 * - 由 YanMainMenuWidget 压入 UI.Layer.Menu 栈。
 * - 由 YanMainMenuWidget 压入 UI.Layer.Menu 栈。
 * - 按 ESC/Back 时自动弹出，返回主菜单。
 *
 * UMG 布局需绑定：
 *   UButton    HostButton  —— 点击创建房间
 *   UTextBlock StatusText  —— 显示当前状态
 */
class UYanHostSessionWidget : UYanUserWidget
{
	UPROPERTY(BindWidget)
	UButton HostButton;

	UPROPERTY(BindWidget)
	UTextBlock StatusText;

	/**
	 * 要托管的 Experience 数据资产（包含地图、最大人数、ExperienceID 等）。
	 * 在子 Blueprint 的 Details 面板中指定一个 UModularUserFacingExperienceDefinition 资产。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Session")
	UModularUserFacingExperienceDefinition UserFacingExperienceDefinition;

	// 启用 ESC/Back 动作接收，允许 CommonUI 将 Back 路由到本 Widget
	default bIsBackHandler = true;
	default bIsBackActionDisplayedInActionBar = true;

	private UYanSessionManager SessionManager;

	UFUNCTION(BlueprintOverride)
	void PreConstruct(bool IsDesignTime)
	{
		if (IsDesignTime)
		{
			return;
		}
		SessionManager = Cast<UYanSessionManager>(Subsystem::GetGameInstanceSubsystem(UYanSessionManager));

		HostButton.OnClicked.AddUFunction(this, n"OnHostClicked");

		if (SessionManager != nullptr)
		{
			SessionManager.OnSessionCreated.AddUFunction(this, n"OnSessionCreated");
		}
	}

	// 点击：创建房间
	UFUNCTION()
	private void OnHostClicked()
	{
		if (SessionManager == nullptr)
		{
			SetStatus("错误：YanSessionManager 不可用");
			return;
		}

		if (UserFacingExperienceDefinition == nullptr)
		{
			SetStatus("错误：未配置 ExperienceDefinition");
			return;
		}

		APlayerController PC = GetOwningPlayer();
		if (PC == nullptr)
		{
			SetStatus("错误：无法获取 PlayerController");
			return;
		}

		SetStatus("正在创建房间…");
		HostButton.SetIsEnabled(false);

		SessionManager.HostSession(PC, UserFacingExperienceDefinition);
	}

	// ── 回调：Manager 广播的创建结果 ─────────────────────────────────────────

	UFUNCTION()
	private void OnSessionCreated(bool bSuccess, FText Message)
	{
		SetStatus(Message.ToString());

		if (!bSuccess)
		{
			// 失败时恢复按钮，允许重试
			HostButton.SetIsEnabled(true);
		}
		// 成功时由 YanMainMenuWidget 监听同一事件并负责关闭整个菜单栈
	}

	//~Begin UCommonActivatableWidget Interface
	/** ESC/Back：关闭本子页面，弹出栈，自动返回主菜单 */
	UFUNCTION(BlueprintOverride)
	bool OnHandleBackAction()
	{
		DeactivateWidget();
		return true;
	}
	//~End UCommonActivatableWidget Interface

	private void SetStatus(FString Msg)
	{
		if (StatusText != nullptr)
		{
			StatusText.SetText(FText::FromString(Msg));
		}
	}
}
