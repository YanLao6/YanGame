// Copyright YanLao.

#include "YanLobbyComponent.h"

#include "Algo/Count.h"
#include "CommonSessionSubsystem.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameMode/ModularUserFacingExperienceDefinition.h"
#include "ModularTeamDisplayAsset.h"
#include "Net/UnrealNetwork.h"
#include "YanLobbyPlayerComponent.h"
#include "YanSessionManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanLobbyComponent)

DEFINE_LOG_CATEGORY_STATIC(LogYanLobby, Log, All);

namespace YanLobby
{
	// 在场的参与者：排除纯观战者与已断开但仍保留 PlayerState 的玩家
	static bool IsActivePlayer(const APlayerState* PlayerState)
	{
		return PlayerState && !PlayerState->IsOnlyASpectator() && !PlayerState->IsInactive();
	}
}

UYanLobbyComponent::UYanLobbyComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UYanLobbyComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	// 房主经 HostViaLobby 选定的对局优先；未经此路径进入大厅（如编辑器直接打开大厅关卡）时退回默认对局
	const UYanSessionManager* const SessionManager = GetWorld()->GetGameInstance()->GetSubsystem<UYanSessionManager>();
	MatchRequest = SessionManager ? SessionManager->GetPendingMatchRequest() : nullptr;
	if (!MatchRequest && NextExperience)
	{
		MatchRequest = NextExperience->CreateHostingRequest(this);
	}

	// 按对局的人数上限均分，使每支队伍在对局里都能容纳所选的人
	if (MatchRequest && Teams.Num() > 0)
	{
		TeamCapacity = FMath::DivideAndRoundUp(MatchRequest->GetMaxPlayers(), Teams.Num());
	}

	LogoutHandle = FGameModeEvents::GameModeLogoutEvent.AddUObject(this, &ThisClass::HandlePlayerLogout);
}

void UYanLobbyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FGameModeEvents::GameModeLogoutEvent.Remove(LogoutHandle);

	Super::EndPlay(EndPlayReason);
}

void UYanLobbyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, TeamCapacity);
	DOREPLIFETIME(ThisClass, ChatMessages);
}

// ── 开局 ──────────────────────────────────────────────────────────────────────

void UYanLobbyComponent::RequestStartMatch()
{
	if (!HasAuthority())
	{
		UE_LOG(LogYanLobby, Warning, TEXT("非 Authority 请求开局，已忽略。开局只能由房主发起。"));
		return;
	}

	if (!CanStartMatch())
	{
		return;
	}

	StartMatchTravel();
}

bool UYanLobbyComponent::CanStartMatch() const
{
	if (bMatchTravelStarted || !MatchRequest)
	{
		return false;
	}

	// 全员都在观战席也允许开局，此时所有人以自由视角观察地图
	return GetActivePlayerCount() >= MinPlayerCount;
}

void UYanLobbyComponent::EvaluateAutoStart()
{
	if (!HasAuthority() || !CanStartMatch())
	{
		return;
	}

	for (const APlayerState* const PlayerState : GetGameStateChecked<AGameStateBase>()->PlayerArray)
	{
		if (!YanLobby::IsActivePlayer(PlayerState))
		{
			continue;
		}

		// 未分配的玩家同样要准备，表示接受开局后观战
		const UYanLobbyPlayerComponent* const LobbyPlayer = UYanLobbyPlayerComponent::FindLobbyPlayer(PlayerState);
		if (!LobbyPlayer || !LobbyPlayer->IsReady())
		{
			return;
		}
	}

	UE_LOG(LogYanLobby, Log, TEXT("全员已准备，自动开局。"));
	StartMatchTravel();
}

void UYanLobbyComponent::StartMatchTravel()
{
	UWorld* const World = GetWorld();
	if (bMatchTravelStarted || !World)
	{
		return;
	}

	// 复用 Hosting 请求构造 URL：其中的 Experience 参数正是对局关卡据以加载 Experience 的依据
	FString URL = MatchRequest ? MatchRequest->ConstructTravelURL() : FString();
	if (URL.IsEmpty())
	{
		UE_LOG(LogYanLobby,
		       Warning,
		       TEXT("对局请求 [%s] 未能构造有效的切图 URL，请检查其 MapID 是否为已注册的主资产。"),
		       MatchRequest ? *MatchRequest->ModeNameForAdvertisement : TEXT("None"));
		return;
	}

	// PlayerState 随切图销毁，选队结果须交给跨关卡存续的会话管理器，并限定只在目标对局关卡生效
	if (UYanSessionManager* const SessionManager = World->GetGameInstance()->GetSubsystem<UYanSessionManager>())
	{
		TMap<FString, int32> Roster;
		for (const APlayerState* const PlayerState : GetGameStateChecked<AGameStateBase>()->PlayerArray)
		{
			const UYanLobbyPlayerComponent* const LobbyPlayer = UYanLobbyPlayerComponent::FindLobbyPlayer(PlayerState);
			if (LobbyPlayer && !PlayerState->IsInactive())
			{
				Roster.Add(UYanSessionManager::GetRosterKey(PlayerState), LobbyPlayer->GetTeamId());
			}
		}

		SessionManager->SetMatchRoster(MoveTemp(Roster), MatchRequest->GetMapName());
	}

	// 必须无缝切图：非无缝切图会在同一帧内为新关卡重新监听，而 SteamSockets 的旧监听 socket
	// 要到下一次 Tick 才真正关闭，新监听因端口占用失败，房主被退回前端。无缝切图全程保留 NetDriver，客机也无需重连。
	// PIE 默认禁用无缝切图（net.AllowPIESeamlessTravel），会自动退化为非无缝，PIE 使用的 IpNetDriver 不受上述问题影响
	URL += TEXT("?SeamlessTravel");

	bMatchTravelStarted = true;

	UE_LOG(LogYanLobby, Log, TEXT("开局，切换到对局关卡 [%s]。"), *URL);

	World->ServerTravel(URL);
}

// ── 队伍 ──────────────────────────────────────────────────────────────────────

TArray<int32> UYanLobbyComponent::GetTeamIds() const
{
	TArray<int32> TeamIds;
	TeamIds.Reserve(Teams.Num());

	for (const TPair<uint8, TObjectPtr<UModularTeamDisplayAsset>>& Team : Teams)
	{
		TeamIds.Add(Team.Key);
	}

	TeamIds.Sort();

	return TeamIds;
}

UModularTeamDisplayAsset* UYanLobbyComponent::GetTeamDisplayAsset(int32 TeamId) const
{
	return IsValidTeamId(TeamId) ? Teams.FindChecked(static_cast<uint8>(TeamId)).Get() : nullptr;
}

TArray<APlayerState*> UYanLobbyComponent::GetPlayersInTeam(int32 TeamId) const
{
	TArray<APlayerState*> Players;

	const AGameStateBase* const GameState = GetGameState<AGameStateBase>();
	if (!GameState)
	{
		return Players;
	}

	for (APlayerState* const PlayerState : GameState->PlayerArray)
	{
		const UYanLobbyPlayerComponent* const LobbyPlayer = UYanLobbyPlayerComponent::FindLobbyPlayer(PlayerState);
		if (LobbyPlayer && !PlayerState->IsInactive() && LobbyPlayer->GetTeamId() == TeamId)
		{
			Players.Add(PlayerState);
		}
	}

	return Players;
}

int32 UYanLobbyComponent::GetActivePlayerCount() const
{
	const AGameStateBase* const GameState = GetGameState<AGameStateBase>();
	if (!GameState)
	{
		return 0;
	}

	return Algo::CountIf(GameState->PlayerArray, &YanLobby::IsActivePlayer);
}

bool UYanLobbyComponent::IsValidTeamId(int32 TeamId) const
{
	return TeamId >= 0 && TeamId <= MAX_uint8 && Teams.Contains(static_cast<uint8>(TeamId));
}

// ── 玩家请求 ──────────────────────────────────────────────────────────────────

void UYanLobbyComponent::AuthJoinTeam(UYanLobbyPlayerComponent* Player, int32 TeamId)
{
	if (!Player || bMatchTravelStarted || Player->GetTeamId() == TeamId)
	{
		return;
	}

	if (TeamId != INDEX_NONE)
	{
		if (!IsValidTeamId(TeamId))
		{
			return;
		}

		if (TeamCapacity > 0 && GetPlayersInTeam(TeamId).Num() >= TeamCapacity)
		{
			return;
		}
	}

	Player->AuthSetTeamId(TeamId);

	// 换队后须重新确认准备，避免自动开局前一刻换到对方队伍
	Player->AuthSetReady(false);
}

void UYanLobbyComponent::AuthSetReady(UYanLobbyPlayerComponent* Player, bool bReady)
{
	if (!Player || bMatchTravelStarted || Player->IsReady() == bReady)
	{
		return;
	}

	Player->AuthSetReady(bReady);

	EvaluateAutoStart();
}

void UYanLobbyComponent::AuthAddChatMessage(const APlayerState* Sender, const FString& Message)
{
	FString Text = Message.TrimStartAndEnd();
	if (!Sender || Text.IsEmpty())
	{
		return;
	}

	Text.LeftInline(MaxChatMessageLength);

	FYanLobbyChatMessage& Entry = ChatMessages.AddDefaulted_GetRef();
	Entry.SenderName = Sender->GetPlayerName();
	Entry.Text = MoveTemp(Text);
	Entry.Serial = NextChatSerial++;

	if (ChatMessages.Num() > MaxChatMessages)
	{
		ChatMessages.RemoveAt(0, ChatMessages.Num() - MaxChatMessages);
	}
}

void UYanLobbyComponent::HandlePlayerLogout(AGameModeBase* GameMode, AController* Exiting)
{
	if (GameMode && GameMode->GetWorld() == GetWorld())
	{
		// 登出广播时离开者仍在 PlayerArray 中，下一帧再判断
		GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::EvaluateAutoStart));
	}
}
