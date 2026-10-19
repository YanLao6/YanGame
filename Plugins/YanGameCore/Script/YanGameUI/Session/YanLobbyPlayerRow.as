/**
 * YanLobbyPlayerRow — 等待大厅中的一行玩家信息：房主标记、名字、准备状态。
 *
 * 由 YanLobbyTeamPanel 按需创建并复用，数据由面板每次刷新时推入，本控件不自行轮询。
 *
 * UMG 布局需绑定：
 *   UTextBlock NameText  —— 玩家名
 * 可选绑定：
 *   UWidget    HostIcon  —— 房主标记，非房主时折叠
 *   UWidget    ReadyIcon —— 已准备标记，未准备时折叠
 */
class UYanLobbyPlayerRow : UUserWidget
{
	UPROPERTY(BindWidget)
	UTextBlock NameText;

	UPROPERTY(BindWidgetOptional)
	UWidget HostIcon;

	UPROPERTY(BindWidgetOptional)
	UWidget ReadyIcon;

	/** 本机玩家名字的颜色，便于在列表中找到自己 */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	FLinearColor LocalPlayerColor = FLinearColor(1.0, 0.8, 0.25, 1.0);

	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	FLinearColor OtherPlayerColor = FLinearColor::White;

	void SetPlayer(APlayerState PlayerState, bool bIsLocalPlayer)
	{
		UYanLobbyPlayerComponent LobbyPlayer = UYanLobbyPlayerComponent::FindLobbyPlayer(PlayerState);

		NameText.SetText(FText::FromString(PlayerState != nullptr ? PlayerState.GetPlayerName() : ""));
		NameText.SetColorAndOpacity(FSlateColor(bIsLocalPlayer ? LocalPlayerColor : OtherPlayerColor));

		SetShown(HostIcon, LobbyPlayer != nullptr && LobbyPlayer.IsHost());
		SetShown(ReadyIcon, LobbyPlayer != nullptr && LobbyPlayer.IsReady());
	}

	private void SetShown(UWidget Widget, bool bShown)
	{
		if (Widget != nullptr)
		{
			Widget.SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}
