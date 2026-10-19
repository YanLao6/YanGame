/**
 * YanGameMenuWidget — 游戏内 ESC 菜单
 *
 * 职责：
 * - 由 UYanHUDLayout 响应 UI.Action.Escape 时压入 UI.Layer.Menu。
 * - 提供继续游戏 / 游戏设置 / 返回主菜单三个入口。
 * - bIsBackHandler = true：再次按 ESC 触发 CommonUI 的 Back 动作停用本界面，
 *   Stack 监听 OnDeactivated 自动出栈，输入交还 HUD。
 *
 * UMG 布局需绑定（变量名需一致，控件须为 CommonButtonBase 子类，如 W_LyraMenuButton）：
 *   - UCommonButtonBase ResumeButton
 *   - UCommonButtonBase SettingsButton
 *   - UCommonButtonBase ReturnToMainMenuButton
 * 三者均为可选绑定：缺失的按钮只是不出现，不影响 ESC 开关本界面。
 *
 * Blueprint Details 设置：
 *   - SettingsScreenClass → W_LyraSettingScreen
 */
class UYanGameMenuWidget : UModularInputActivatableWidget
{
	// 激活期间切到 Menu 输入模式：释放鼠标、屏蔽游戏输入
	default InputConfig = EModularInputWidgetInputMode::Menu;

	// 由 CommonUI 自动注册 Back 绑定（DefaultBackAction → GenericBack，含 Escape）
	default bIsBackHandler = true;

	UPROPERTY(BindWidgetOptional)
	UCommonButtonBase ResumeButton;

	UPROPERTY(BindWidgetOptional)
	UCommonButtonBase SettingsButton;

	UPROPERTY(BindWidgetOptional)
	UCommonButtonBase ReturnToMainMenuButton;

	/** 设置界面，压入同一 Menu 层的上一格，ESC 逐级回退 */
	UPROPERTY(EditDefaultsOnly, Category = "UI|SubPages")
	TSubclassOf<UCommonActivatableWidget> SettingsScreenClass;

	UFUNCTION(BlueprintOverride)
	void PreConstruct(bool IsDesignTime)
	{
		if (IsDesignTime)
		{
			return;
		}

		if (ResumeButton != nullptr)
		{
			ResumeButton.OnButtonBaseClicked.AddUFunction(this, n"OnResumeClicked");
		}

		if (SettingsButton != nullptr)
		{
			SettingsButton.OnButtonBaseClicked.AddUFunction(this, n"OnSettingsClicked");
		}

		if (ReturnToMainMenuButton != nullptr)
		{
			ReturnToMainMenuButton.OnButtonBaseClicked.AddUFunction(this, n"OnReturnToMainMenuClicked");
		}
	}

	// 手柄/键盘导航的初始焦点
	UFUNCTION(BlueprintOverride)
	UWidget BP_GetDesiredFocusTarget() const
	{
		return ResumeButton;
	}

	UFUNCTION()
	private void OnResumeClicked(UCommonButtonBase Button)
	{
		DeactivateWidget();
	}

	UFUNCTION()
	private void OnSettingsClicked(UCommonButtonBase Button)
	{
		if (SettingsScreenClass.Get() == nullptr)
		{
			return;
		}

		UCommonUIExtensions::PushContentToLayer_ForPlayer(
			GetOwningLocalPlayer(),
			FGameplayTag::RequestGameplayTag(n"UI.Layer.Menu"),
			SettingsScreenClass);
	}

	UFUNCTION()
	private void OnReturnToMainMenuClicked(UCommonButtonBase Button)
	{
		// 回到默认地图（前端）；L_Lobby 是等待大厅而非前端，不能作为返回目标
		UYanSessionManager SessionManager = Cast<UYanSessionManager>(Subsystem::GetGameInstanceSubsystem(UYanSessionManager));
		if (SessionManager != nullptr)
		{
			SessionManager.ReturnToMainMenu();
		}
	}
}
