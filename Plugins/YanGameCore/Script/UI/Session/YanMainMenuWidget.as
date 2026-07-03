/**
 * YanMainMenuWidget — 主菜单界面（Main Screen）
 *
 * 用途：
 * - 作为 `UYanFrontendStateComponent::MainScreenClass` 对应的 UI。
 * - 展示 Host / Find&Join 两个按钮；点击后将子页面压入 UI.Layer.Menu 栈。
 * - 子页面（YanHostSessionWidget / YanSearchSessionWidget）独立激活，
 *   玩家按 ESC / Back 时子页面自行弹出，无需本类干预。
 *
 * UMG 布局需绑定（变量名需一致）：
 *   - UButton HostButton
 *   - UButton FindanJoinButton
 *
 * Blueprint Details 设置：
 *   - HostWidgetClass    → WBP_YanHostSessionWidget（或其子类）
 *   - SearchWidgetClass  → WBP_YanSearchSessionWidget（或其子类）
 *   - bIsBackHandler     → false（主菜单不消费 ESC，防止意外关闭）
 */
class UYanMainMenuWidget : UYanUserWidget
{
	UPROPERTY(BindWidget)
	UButton HostButton;

	UPROPERTY(BindWidget)
	UButton FindanJoinButton;

	/** 创建房间子页面 */
	UPROPERTY(EditDefaultsOnly, Category = "UI|SubPages")
	TSubclassOf<UYanHostSessionWidget> HostWidgetClass;

	/** 搜索房间子页面 */
	UPROPERTY(EditDefaultsOnly, Category = "UI|SubPages")
	TSubclassOf<UYanSearchSessionWidget> SearchWidgetClass;


	// 用于监听 Host/Join 成功事件
	private UYanSessionManager SessionManager;

	// Travel 关闭去重标记
	private bool bClosedForTravel = false;

	UFUNCTION(BlueprintOverride)
	void PreConstruct(bool IsDesignTime)
	{
		if (IsDesignTime)
		{
			return;
		}

		HostButton.OnClicked.AddUFunction(this, n"OnHostButtonClicked");
		FindanJoinButton.OnClicked.AddUFunction(this, n"OnFindAndJoinButtonClicked");

		SessionManager = Cast<UYanSessionManager>(Subsystem::GetGameInstanceSubsystem(UYanSessionManager));
		if (SessionManager != nullptr)
		{
			SessionManager.OnSessionCreated.AddUFunction(this, n"OnSessionCreatedForTravel");
			SessionManager.OnSessionJoined.AddUFunction(this, n"OnSessionJoinedForTravel");
		}
	}

	UFUNCTION()
	private void OnHostButtonClicked()
	{
		UCommonUIExtensions::PushContentToLayer_ForPlayer(GetOwningLocalPlayer(),FGameplayTag::RequestGameplayTag(n"UI.Layer.Menu"), HostWidgetClass);
	}

	UFUNCTION()
	private void OnFindAndJoinButtonClicked()
	{
		UCommonUIExtensions::PushContentToLayer_ForPlayer(GetOwningLocalPlayer(),FGameplayTag::RequestGameplayTag(n"UI.Layer.Menu"), SearchWidgetClass);
	}

	// Host 成功 → 准备 Travel，收起前端菜单
	UFUNCTION()
	private void OnSessionCreatedForTravel(bool bSuccess, FText Message)
	{
		TryCloseForTravel(bSuccess);
	}

	// Join 成功 → 等待 ClientTravel，收起前端菜单
	UFUNCTION()
	private void OnSessionJoinedForTravel(bool bSuccess, FText Message)
	{
		TryCloseForTravel(bSuccess);
	}

	/**
	 * Travel 前关闭整个菜单栈。
	 * 先弹出子页面（若存在），再弹出本 Widget，
	 * YanFrontendStateComponent 监听本 Widget 关闭后会结束 ControlFlow。
	 */
	private void TryCloseForTravel(bool bSuccess)
	{
		if (!bSuccess || bClosedForTravel)
		{
			return;
		}

		bClosedForTravel = true;

		// 关闭主菜单，触发 FrontendStateComponent ControlFlow 继续
		DeactivateWidget();
	}
}
