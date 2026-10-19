/**
 * YanLobbyTeamPanel — 等待大厅中的一支队伍（或未分配区）：标题、人数与玩家列表。
 *
 * TeamId 为 -1 时表示未分配区。双击面板即请求加入该队伍，在未分配区双击等于离开队伍。
 * 名称与颜色取自大厅组件配置的队伍展示资产（TeamShortName 与 ColorParameters 中的 TeamColor）。
 *
 * UMG 布局需绑定：
 *   UTextBlock   TitleText  —— 队伍名
 *   UPanelWidget PlayerList —— 玩家行的容器（通常为 VerticalBox）
 * 可选绑定：
 *   UTextBlock   CountText  —— 人数，如 "2/4"
 *   UBorder      HeaderBorder —— 标题底色，按队伍颜色着色
 */
class UYanLobbyTeamPanel : UUserWidget
{
	UPROPERTY(BindWidget)
	UTextBlock TitleText;

	UPROPERTY(BindWidget)
	UPanelWidget PlayerList;

	UPROPERTY(BindWidgetOptional)
	UTextBlock CountText;

	UPROPERTY(BindWidgetOptional)
	UBorder HeaderBorder;

	/** 玩家行的控件类，指向 WBP_LobbyPlayerRow */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	TSubclassOf<UYanLobbyPlayerRow> RowClass;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	FText UnassignedTitle = FText::FromString("未分配");

	/** 未分配区与缺少颜色配置的队伍使用的标题底色 */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	FLinearColor NeutralColor = FLinearColor(0.12, 0.14, 0.18, 0.9);

	/** 所代表的队伍编号，-1 为未分配区；由大厅界面在创建后设置 */
	int32 TeamId = -1;

	private TArray<UYanLobbyPlayerRow> Rows;
	private bool bHeaderInitialized = false;

	void Refresh(UYanLobbyComponent Lobby)
	{
		if (Lobby == nullptr)
		{
			return;
		}

		// 队伍名与颜色来自配置，不随对局状态变化，写一次即可
		if (!bHeaderInitialized)
		{
			RefreshHeader(Lobby);
			bHeaderInitialized = true;
		}

		TArray<APlayerState> Players = Lobby.GetPlayersInTeam(TeamId);
		RefreshCount(Lobby, Players.Num());
		RefreshRows(Players);
	}

	//~Begin UUserWidget Interface
	UFUNCTION(BlueprintOverride)
	FEventReply OnMouseButtonDoubleClick(FGeometry MyGeometry, FPointerEvent MouseEvent)
	{
		RequestJoin();
		return FEventReply::Handled();
	}
	//~End UUserWidget Interface

	/** 请求把本机玩家移入本面板代表的队伍；满员等情形由服务器拒绝 */
	UFUNCTION()
	void RequestJoin()
	{
		APlayerController PC = GetOwningPlayer();
		UYanLobbyPlayerComponent LocalPlayer = PC != nullptr ? UYanLobbyPlayerComponent::FindLobbyPlayer(PC.PlayerState) : nullptr;
		if (LocalPlayer != nullptr && LocalPlayer.GetTeamId() != TeamId)
		{
			LocalPlayer.RequestJoinTeam(TeamId);
		}
	}

	private void RefreshHeader(UYanLobbyComponent Lobby)
	{
		UModularTeamDisplayAsset DisplayAsset = TeamId >= 0 ? Lobby.GetTeamDisplayAsset(TeamId) : nullptr;

		FText Title = UnassignedTitle;
		if (TeamId >= 0)
		{
			Title = DisplayAsset != nullptr && !DisplayAsset.TeamShortName.IsEmpty()
				? DisplayAsset.TeamShortName
				: FText::FromString("队伍 " + String::Conv_IntToString(TeamId));
		}
		TitleText.SetText(Title);

		if (HeaderBorder != nullptr)
		{
			HeaderBorder.SetBrushColor(ModularTeam::GetTeamColorWithFallback(DisplayAsset, n"TeamColor", NeutralColor));
		}
	}

	private void RefreshCount(UYanLobbyComponent Lobby, int32 PlayerCount)
	{
		if (CountText == nullptr)
		{
			return;
		}

		const int32 Capacity = Lobby.GetTeamCapacity();
		FString Count = String::Conv_IntToString(PlayerCount);
		if (TeamId >= 0 && Capacity > 0)
		{
			Count += "/" + String::Conv_IntToString(Capacity);
		}
		CountText.SetText(FText::FromString(Count));
	}

	// 行控件按需创建并复用，多余的折叠而不销毁，避免每次刷新重建整列
	private void RefreshRows(TArray<APlayerState> Players)
	{
		if (RowClass.Get() == nullptr)
		{
			return;
		}

		APlayerController PC = GetOwningPlayer();
		APlayerState LocalPlayerState = PC != nullptr ? PC.PlayerState : nullptr;

		while (Rows.Num() < Players.Num())
		{
			UYanLobbyPlayerRow NewRow = Cast<UYanLobbyPlayerRow>(WidgetBlueprint::CreateWidget(RowClass, PC));
			PlayerList.AddChild(NewRow);
			Rows.Add(NewRow);
		}

		for (int32 Index = 0; Index < Rows.Num(); Index++)
		{
			if (Index < Players.Num())
			{
				Rows[Index].SetPlayer(Players[Index], Players[Index] == LocalPlayerState);
				Rows[Index].SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
			else
			{
				Rows[Index].SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}
