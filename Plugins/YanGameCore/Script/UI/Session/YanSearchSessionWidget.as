/**
 * YanSearchSessionWidget — 搜索并加入 Steam 联机房间
 *
 * 通过 UYanSessionManager 进行 FindSessions / JoinSession 两个流程。
 * 搜索结果存放在 Manager.CachedResults，添加到 UListView 展示。
 *
 * 作为独立的 CommonUI 栈页面使用：
 * - 由 YanMainMenuWidget 压入 UI.Layer.Menu 栈。
 * - 按 ESC/Back 时自动弹出，返回主菜单。
 *
 * UMG 布局需绑定：
 *   UButton    SearchButton    —— 点击开始搜索
 *   UButton    JoinButton      —— 点击加入当前选中的房间
 *   UListView  SessionListView —— 展示搜索结果；EntryWidgetClass 设为 WBP_YanSessionEntryWidget
 *   UTextBlock StatusText      —— 显示当前状态
 */
class UYanSearchSessionWidget : UYanUserWidget
{
	UPROPERTY(BindWidget)
	UButton SearchButton;

	UPROPERTY(BindWidget)
	UButton JoinButton;

	UPROPERTY(BindWidget)
	UListView SessionListView;

	UPROPERTY(BindWidget)
	UTextBlock StatusText;

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

		SearchButton.OnClicked.AddUFunction(this, n"OnSearchClicked");
		JoinButton.OnClicked.AddUFunction(this, n"OnJoinClicked");

		// 初始状态：未选中时加入不可用
		JoinButton.SetIsEnabled(false);

		// 监听 ListView 的选中变化
		SessionListView.BP_OnItemSelectionChanged.AddUFunction(this, n"OnSessionSelected");

		if (SessionManager != nullptr)
		{
			SessionManager.OnSearchCompleted.AddUFunction(this, n"OnSearchCompleted");
			SessionManager.OnSessionJoined.AddUFunction(this, n"OnSessionJoined");
		}
	}

	UFUNCTION()
	private void OnSearchClicked()
	{
		if (SessionManager == nullptr)
		{
			SetStatus("错误：YanSessionManager 不可用");
			return;
		}

		APlayerController PC = GetOwningPlayer();
		if (PC == nullptr)
		{
			SetStatus("错误：无法获取 PlayerController");
			return;
		}

		SessionListView.ClearListItems();
		JoinButton.SetIsEnabled(false);
		SearchButton.SetIsEnabled(false);
		SetStatus("正在搜索房间…");

		SessionManager.FindSessions(PC);
	}

	UFUNCTION()
	private void OnSearchCompleted(bool bSuccess, FText Message)
	{
		SearchButton.SetIsEnabled(true);
		SetStatus(Message.ToString());

		if (!bSuccess || SessionManager == nullptr)
		{
			return;
		}

		// 将 Manager 缓存的结果添加到 ListView
		// CachedResults 的每个元素是 UYanSessionSearchResultData，
		// 由 YanSessionEntryWidget 的 OnListItemObjectSet 负责渲染
		int32 Count = SessionManager.CachedResults.Num();
		for (int32 i = 0; i < Count; i++)
		{
			SessionListView.AddItem(SessionManager.CachedResults[i]);
		}
	}

	UFUNCTION()
	private void OnSessionSelected(UObject Item, bool bIsSelected)
	{
		JoinButton.SetIsEnabled(SessionListView.GetSelectedItem() != nullptr);
	}

	UFUNCTION()
	private void OnJoinClicked()
	{
		if (SessionManager == nullptr)
		{
			SetStatus("错误：YanSessionManager 不可用");
			return;
		}

		APlayerController PC = GetOwningPlayer();
		if (PC == nullptr)
		{
			SetStatus("错误：无法获取 PlayerController");
			return;
		}

		// 从 ListView 取出当前选中的条目（UYanSessionSearchResultData）
		UYanSessionSearchResultData SelectedData = Cast<UYanSessionSearchResultData>(SessionListView.GetSelectedItem());

		if (SelectedData == nullptr)
		{
			SetStatus("请先选中一个房间");
			return;
		}

		SetStatus("正在加入房间…");
		JoinButton.SetIsEnabled(false);
		SearchButton.SetIsEnabled(false);

		// JoinSessionByData 内部调用 JoinSession(PC, SelectedData.SearchResult)
		SessionManager.JoinSessionByData(PC, SelectedData);
	}

	UFUNCTION()
	private void OnSessionJoined(bool bSuccess, FText Message)
	{
		SetStatus(Message.ToString());

		if (!bSuccess)
		{
			JoinButton.SetIsEnabled(true);
			SearchButton.SetIsEnabled(true);
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
