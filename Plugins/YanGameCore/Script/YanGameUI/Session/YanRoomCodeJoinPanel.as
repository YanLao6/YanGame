/**
 * YanRoomCodeJoinPanel — 凭房间码加入房间，玩家之间无需好友关系。
 *
 * 作为普通控件嵌入房间浏览界面（W_SessionBrowserScreen），不单独压栈。
 * 格式校验、查找与加入都由 UYanSessionManager 完成，结果经 OnSessionJoined 返回；
 * 成功后客户端随即切图，本控件随原关卡一同销毁。
 *
 * UMG 布局需绑定：
 *   UEditableTextBox   RoomCodeInput —— 输入房间码，回车等同点击加入
 *   UModularButtonBase JoinButton    —— 加入
 * 可选绑定：
 *   UTextBlock         StatusText    —— 查找与失败提示
 */
class UYanRoomCodeJoinPanel : UUserWidget
{
	UPROPERTY(BindWidget)
	UEditableTextBox RoomCodeInput;

	UPROPERTY(BindWidget)
	UModularButtonBase JoinButton;

	UPROPERTY(BindWidgetOptional)
	UTextBlock StatusText;

	private UYanSessionManager SessionManager;
	private bool bJoining = false;

	//~Begin UUserWidget Interface
	UFUNCTION(BlueprintOverride)
	void Construct()
	{
		JoinButton.OnButtonBaseClicked.AddUFunction(this, n"HandleJoinClicked");
		RoomCodeInput.OnTextCommitted.AddUFunction(this, n"HandleRoomCodeCommitted");

		SessionManager = Cast<UYanSessionManager>(Subsystem::GetGameInstanceSubsystem(UYanSessionManager));
		if (SessionManager != nullptr)
		{
			SessionManager.OnSessionJoined.AddUFunction(this, n"HandleSessionJoined");
		}

		SetStatus("");
	}

	/** Manager 是 GameInstance 子系统，存活时间长于本控件，绑定必须在此解除 */
	UFUNCTION(BlueprintOverride)
	void Destruct()
	{
		if (SessionManager != nullptr)
		{
			SessionManager.OnSessionJoined.UnbindObject(this);
		}
	}
	//~End UUserWidget Interface

	/** 按输入框中的房间码发起加入 */
	UFUNCTION()
	void JoinByRoomCode()
	{
		if (bJoining)
		{
			return;
		}

		if (SessionManager == nullptr)
		{
			SetStatus("会话服务不可用");
			return;
		}

		SetJoining(true);
		SetStatus("正在查找房间…");

		// 格式不合法、房间不存在都由 Manager 经 OnSessionJoined 报告
		SessionManager.JoinSessionByRoomCode(GetOwningPlayer(), RoomCodeInput.GetText().ToString());
	}

	UFUNCTION()
	private void HandleJoinClicked(UCommonButtonBase Button)
	{
		JoinByRoomCode();
	}

	UFUNCTION()
	private void HandleRoomCodeCommitted(const FText&in Text, ETextCommit CommitMethod)
	{
		if (CommitMethod == ETextCommit::OnEnter)
		{
			JoinByRoomCode();
		}
	}

	// 列表浏览的加入也会触发同一事件，只处理本控件发起的那一次
	UFUNCTION()
	private void HandleSessionJoined(bool bSuccess, FText Message)
	{
		if (!bJoining)
		{
			return;
		}

		SetStatus(Message.ToString());

		if (!bSuccess)
		{
			SetJoining(false);
		}
	}

	private void SetJoining(bool bInJoining)
	{
		bJoining = bInJoining;
		JoinButton.SetIsEnabled(!bInJoining);
		RoomCodeInput.SetIsEnabled(!bInJoining);
	}

	private void SetStatus(FString Message)
	{
		if (StatusText != nullptr)
		{
			StatusText.SetText(FText::FromString(Message));
		}
	}
}
