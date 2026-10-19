/**
 * YanLobbyWidget — 等待大厅界面。
 *
 * 房主开房或玩家凭码加入后停留于此：
 *   左侧   未分配区（新加入的玩家默认在此）与聊天栏
 *   中间   各队伍面板，双击加入；按大厅组件配置的队伍动态生成
 *   底部   准备、开始（仅房主）、离开队伍、退出房间
 *   顶部   房间码与人数
 * 全员准备后由服务器自动开局；房主的开始按钮无视准备强制开局。开局时仍未分配的玩家进入观战。
 *
 * 大厅数据全部来自复制的 UYanLobbyComponent / UYanLobbyPlayerComponent，
 * 两者随复制到达，界面构建时未必就绪，故以定时器轮询刷新并兼做重试。
 *
 * UMG 布局需绑定（变量名需一致）：
 *   UYanLobbyTeamPanel UnassignedPanel  —— 未分配区
 *   UPanelWidget       TeamPanelBox     —— 队伍面板的容器
 *   UModularButtonBase ReadyButton      —— 准备 / 取消准备
 *   UModularButtonBase StartMatchButton —— 开始游戏（仅房主可见）
 *   UModularButtonBase LeaveTeamButton  —— 回到未分配
 *   UModularButtonBase LeaveRoomButton  —— 退出房间
 * 可选绑定：
 *   UTextBlock         RoomCodeText, PlayerCountText, StatusText
 *   UModularButtonBase CopyRoomCodeButton, InviteButton
 *   UTextBlock         ChatLogText      —— 聊天记录
 *   UScrollBox         ChatScrollBox    —— 包住聊天记录，新消息到达时滚到底部
 *   UEditableTextBox   ChatInput        —— 回车发送
 */
class UYanLobbyWidget : UYanUserWidget
{
	// 大厅内不接受游戏输入，避免玩家在等待期间操作角色
	default InputConfig = EModularInputWidgetInputMode::Menu;

	// ESC / Back 即退出房间
	default bIsBackHandler = true;

	UPROPERTY(BindWidget)
	UYanLobbyTeamPanel UnassignedPanel;

	UPROPERTY(BindWidget)
	UPanelWidget TeamPanelBox;

	UPROPERTY(BindWidget)
	UModularButtonBase ReadyButton;

	UPROPERTY(BindWidget)
	UModularButtonBase StartMatchButton;

	UPROPERTY(BindWidget)
	UModularButtonBase LeaveTeamButton;

	UPROPERTY(BindWidget)
	UModularButtonBase LeaveRoomButton;

	UPROPERTY(BindWidgetOptional)
	UTextBlock RoomCodeText;

	UPROPERTY(BindWidgetOptional)
	UModularButtonBase CopyRoomCodeButton;

	UPROPERTY(BindWidgetOptional)
	UModularButtonBase InviteButton;

	UPROPERTY(BindWidgetOptional)
	UTextBlock PlayerCountText;

	UPROPERTY(BindWidgetOptional)
	UTextBlock StatusText;

	UPROPERTY(BindWidgetOptional)
	UTextBlock ChatLogText;

	UPROPERTY(BindWidgetOptional)
	UScrollBox ChatScrollBox;

	UPROPERTY(BindWidgetOptional)
	UEditableTextBox ChatInput;

	/** 队伍面板的控件类，指向 WBP_LobbyTeamPanel */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	TSubclassOf<UYanLobbyTeamPanel> TeamPanelClass;

	/** 刷新间隔（秒），同时用于重试获取大厅组件 */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby", Meta = (ClampMin = "0.05"))
	float RefreshInterval = 0.25;

	private UYanSessionManager SessionManager;
	private UYanLobbyComponent LobbyComponent;
	private TArray<UYanLobbyTeamPanel> TeamPanels;

	private FTimerHandle RefreshTimer;
	private int32 LastChatSerial = 0;

	// 状态提示被按钮操作的反馈覆盖后，保持一段时间再回到常规提示
	private FString TransientStatus;
	private float TransientStatusUntil = 0.0;

	//~Begin UUserWidget Interface
	UFUNCTION(BlueprintOverride)
	void Construct()
	{
		ReadyButton.OnButtonBaseClicked.AddUFunction(this, n"HandleReadyClicked");
		StartMatchButton.OnButtonBaseClicked.AddUFunction(this, n"HandleStartMatchClicked");
		LeaveTeamButton.OnButtonBaseClicked.AddUFunction(this, n"HandleLeaveTeamClicked");
		LeaveRoomButton.OnButtonBaseClicked.AddUFunction(this, n"HandleLeaveRoomClicked");

		if (CopyRoomCodeButton != nullptr)
		{
			CopyRoomCodeButton.OnButtonBaseClicked.AddUFunction(this, n"HandleCopyRoomCodeClicked");
		}

		if (InviteButton != nullptr)
		{
			InviteButton.OnButtonBaseClicked.AddUFunction(this, n"HandleInviteClicked");
		}

		if (ChatInput != nullptr)
		{
			ChatInput.OnTextCommitted.AddUFunction(this, n"HandleChatCommitted");
		}

		SessionManager = Cast<UYanSessionManager>(Subsystem::GetGameInstanceSubsystem(UYanSessionManager));

		// 房主身份随大厅数据到达后才能确定，在此之前先隐藏开局按钮
		StartMatchButton.SetVisibility(ESlateVisibility::Collapsed);

		RefreshTimer = System::SetTimer(this, n"Refresh", RefreshInterval, true);
		Refresh();
	}

	UFUNCTION(BlueprintOverride)
	void Destruct()
	{
		System::ClearAndInvalidateTimerHandle(RefreshTimer);
	}
	//~End UUserWidget Interface

	//~Begin UCommonActivatableWidget Interface
	// 手柄/键盘导航的初始焦点
	UFUNCTION(BlueprintOverride)
	UWidget BP_GetDesiredFocusTarget() const
	{
		return ReadyButton;
	}

	UFUNCTION(BlueprintOverride)
	bool OnHandleBackAction()
	{
		LeaveRoom();
		return true;
	}
	//~End UCommonActivatableWidget Interface

	// ── 操作 ─────────────────────────────────────────────────────────────────
	// 不带参数，便于自动化验证用定时器在下一帧触发，走真实的 RPC 路径

	UFUNCTION()
	void ToggleReady()
	{
		UYanLobbyPlayerComponent LocalPlayer = GetLocalLobbyPlayer();
		if (LocalPlayer != nullptr)
		{
			LocalPlayer.RequestSetReady(!LocalPlayer.IsReady());
		}
	}

	UFUNCTION()
	void StartMatch()
	{
		UYanLobbyComponent Lobby = GetLobbyComponent();
		if (Lobby == nullptr || !Lobby.CanStartMatch())
		{
			ShowTransientStatus("暂时无法开始：人数不足或正在进入对局");
			return;
		}

		StartMatchButton.SetIsEnabled(false);
		ShowTransientStatus("正在进入对局…");
		Lobby.RequestStartMatch();
	}

	UFUNCTION()
	void LeaveTeam()
	{
		UnassignedPanel.RequestJoin();
	}

	UFUNCTION()
	void LeaveRoom()
	{
		if (SessionManager != nullptr)
		{
			SessionManager.ReturnToMainMenu();
		}
	}

	// ── 控件事件 ─────────────────────────────────────────────────────────────

	UFUNCTION()
	private void HandleReadyClicked(UCommonButtonBase Button)
	{
		ToggleReady();
	}

	UFUNCTION()
	private void HandleStartMatchClicked(UCommonButtonBase Button)
	{
		StartMatch();
	}

	UFUNCTION()
	private void HandleLeaveTeamClicked(UCommonButtonBase Button)
	{
		LeaveTeam();
	}

	UFUNCTION()
	private void HandleLeaveRoomClicked(UCommonButtonBase Button)
	{
		LeaveRoom();
	}

	UFUNCTION()
	private void HandleCopyRoomCodeClicked(UCommonButtonBase Button)
	{
		const FString Code = GetRoomCode();
		if (Code.IsEmpty())
		{
			ShowTransientStatus("房间码尚未就绪");
			return;
		}

		FPlatformApplicationMisc::ClipboardCopy(Code);
		ShowTransientStatus("房间码 " + Code + " 已复制，发给朋友即可加入");
	}

	UFUNCTION()
	private void HandleInviteClicked(UCommonButtonBase Button)
	{
		if (SessionManager == nullptr || !SessionManager.ShowInviteUI(GetOwningPlayer()))
		{
			// 覆盖层需要 Steam 客户端在运行，且本机已建立会话
			ShowTransientStatus("无法打开 Steam 邀请面板，请确认 Steam 已运行");
		}
	}

	UFUNCTION()
	private void HandleChatCommitted(const FText&in Text, ETextCommit CommitMethod)
	{
		if (CommitMethod != ETextCommit::OnEnter)
		{
			return;
		}

		UYanLobbyPlayerComponent LocalPlayer = GetLocalLobbyPlayer();
		if (LocalPlayer != nullptr && !Text.IsEmpty())
		{
			LocalPlayer.SendChatMessage(Text.ToString());
		}

		ChatInput.SetText(FText());
	}

	// ── 刷新 ─────────────────────────────────────────────────────────────────

	UFUNCTION()
	private void Refresh()
	{
		RefreshRoomInfo();

		UYanLobbyComponent Lobby = GetLobbyComponent();
		UYanLobbyPlayerComponent LocalPlayer = GetLocalLobbyPlayer();
		if (Lobby == nullptr || LocalPlayer == nullptr)
		{
			SetStatus("正在等待大厅数据…");
			return;
		}

		EnsureTeamPanels(Lobby);
		UnassignedPanel.Refresh(Lobby);
		for (UYanLobbyTeamPanel Panel : TeamPanels)
		{
			Panel.Refresh(Lobby);
		}

		RefreshButtons(Lobby, LocalPlayer);
		RefreshChat(Lobby);
		RefreshStatus(LocalPlayer);
	}

	private void RefreshRoomInfo()
	{
		const FString Code = GetRoomCode();

		if (RoomCodeText != nullptr)
		{
			RoomCodeText.SetText(FText::FromString("房间码  " + (Code.IsEmpty() ? "------" : Code)));
		}

		if (CopyRoomCodeButton != nullptr)
		{
			CopyRoomCodeButton.SetIsEnabled(!Code.IsEmpty());
		}

		if (InviteButton != nullptr && SessionManager != nullptr)
		{
			InviteButton.SetIsEnabled(SessionManager.CanInvite());
		}

		if (PlayerCountText != nullptr)
		{
			AGameStateBase GameState = Gameplay::GetGameState();
			const int32 Current = GameState != nullptr ? GameState.PlayerArray.Num() : 0;
			const int32 Max = SessionManager != nullptr ? SessionManager.GetCurrentMaxPlayers() : 0;

			FString Count = "玩家  " + String::Conv_IntToString(Current);
			if (Max > 0)
			{
				Count += "/" + String::Conv_IntToString(Max);
			}
			PlayerCountText.SetText(FText::FromString(Count));
		}
	}

	// 队伍由大厅组件配置决定，组件就绪后只生成一次
	private void EnsureTeamPanels(UYanLobbyComponent Lobby)
	{
		if (TeamPanels.Num() > 0 || TeamPanelClass.Get() == nullptr)
		{
			return;
		}

		for (int32 TeamId : Lobby.GetTeamIds())
		{
			UYanLobbyTeamPanel Panel = Cast<UYanLobbyTeamPanel>(WidgetBlueprint::CreateWidget(TeamPanelClass, GetOwningPlayer()));
			Panel.TeamId = TeamId;
			TeamPanelBox.AddChild(Panel);
			TeamPanels.Add(Panel);
		}
	}

	private void RefreshButtons(UYanLobbyComponent Lobby, UYanLobbyPlayerComponent LocalPlayer)
	{
		ReadyButton.SetButtonText(FText::FromString(LocalPlayer.IsReady() ? "取消准备" : "准备"));
		LeaveTeamButton.SetIsEnabled(LocalPlayer.GetTeamId() >= 0);

		// 非房主永远看不到开局按钮，避免客机点击后无声失败
		StartMatchButton.SetVisibility(LocalPlayer.IsHost() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (LocalPlayer.IsHost())
		{
			StartMatchButton.SetIsEnabled(Lobby.CanStartMatch());
		}
	}

	private void RefreshChat(UYanLobbyComponent Lobby)
	{
		if (ChatLogText == nullptr)
		{
			return;
		}

		const TArray<FYanLobbyChatMessage>& Messages = Lobby.GetChatMessages();
		const int32 NewestSerial = Messages.Num() > 0 ? Messages.Last().Serial : 0;
		if (NewestSerial == LastChatSerial)
		{
			return;
		}
		LastChatSerial = NewestSerial;

		FString Log;
		for (const FYanLobbyChatMessage& Message : Messages)
		{
			if (!Log.IsEmpty())
			{
				Log += "\n";
			}
			Log += Message.SenderName + ": " + Message.Text;
		}
		ChatLogText.SetText(FText::FromString(Log));

		if (ChatScrollBox != nullptr)
		{
			ChatScrollBox.ScrollToEnd();
		}
	}

	private void RefreshStatus(UYanLobbyPlayerComponent LocalPlayer)
	{
		if (!TransientStatus.IsEmpty() && System::GetGameTimeInSeconds() < TransientStatusUntil)
		{
			SetStatus(TransientStatus);
			return;
		}
		TransientStatus = "";

		if (LocalPlayer.GetTeamId() < 0)
		{
			SetStatus(LocalPlayer.IsReady() ? "已准备，开局后将观战" : "双击队伍加入；不加入队伍的玩家开局后观战");
		}
		else if (!LocalPlayer.IsReady())
		{
			SetStatus("准备就绪后点击「准备」，全员准备后自动开局");
		}
		else
		{
			SetStatus(LocalPlayer.IsHost() ? "等待其他玩家准备，也可直接开始" : "已准备，等待其他玩家");
		}
	}

	// ── 工具 ─────────────────────────────────────────────────────────────────

	private void ShowTransientStatus(FString Message)
	{
		TransientStatus = Message;
		TransientStatusUntil = System::GetGameTimeInSeconds() + 3.0;
		SetStatus(Message);
	}

	private void SetStatus(FString Message)
	{
		if (StatusText != nullptr)
		{
			StatusText.SetText(FText::FromString(Message));
		}
	}

	private FString GetRoomCode() const
	{
		return SessionManager != nullptr ? SessionManager.GetCurrentRoomCode() : "";
	}

	// 大厅组件挂在 GameState 上，随复制到达，取到之前每次刷新重试
	private UYanLobbyComponent GetLobbyComponent()
	{
		if (LobbyComponent == nullptr)
		{
			AGameStateBase GameState = Gameplay::GetGameState();
			if (GameState != nullptr)
			{
				LobbyComponent = UYanLobbyComponent::Get(GameState);
			}
		}

		return LobbyComponent;
	}

	private UYanLobbyPlayerComponent GetLocalLobbyPlayer() const
	{
		APlayerController PC = GetOwningPlayer();
		return PC != nullptr ? UYanLobbyPlayerComponent::FindLobbyPlayer(PC.PlayerState) : nullptr;
	}
}
