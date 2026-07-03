class UYanLobbyLogWidget : UYanUserWidget
{
	/** 大厅信息文本（Host/Join 成功并进入地图后展示） */
	UPROPERTY(BindWidget)
	UCommonTextBlock LobbyInfoText;

	private UYanSessionManager SessionManager;
	private FTimerHandle RefreshTimerHandle;

	UFUNCTION(BlueprintOverride)
	void PreConstruct(bool IsDesignTime)
	{
		if (IsDesignTime)
		{
			return;
		}

		SessionManager = Cast<UYanSessionManager>(Subsystem::GetGameInstanceSubsystem(UYanSessionManager));

		// 进入大厅后信息会变化（玩家人数等），用 Timer 周期刷新更稳妥
		RefreshTimerHandle = System::SetTimer(this, n"RefreshLobbyInfo", 0.5f, true);
		RefreshLobbyInfo();
	}

	UFUNCTION()
	private void RefreshLobbyInfo()
	{
		if (LobbyInfoText == nullptr)
		{
			return;
		}

		if (SessionManager == nullptr)
		{
			LobbyInfoText.SetText(FText::FromString("错误：YanSessionManager 不可用"));
			return;
		}

		LobbyInfoText.SetText(SessionManager.GetLobbyInfoText());
	}
}