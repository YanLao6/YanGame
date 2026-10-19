// Copyright YanLao.

#include "YanSessionManager.h"
#include "GameMode/ModularUserFacingExperienceDefinition.h"
#include "ModularSystemStatics.h"
#include "YanGameSessionLogs.h"
#include "YanLoadingScreenSubsystem.h"
#include "YanSessionKeys.h"
#include "YanSessionSettings.h"
#include "CommonUserTypes.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"
#include "Interfaces/OnlineExternalUIInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanSessionManager)

// ── 初始化 / 反初始化 ─────────────────────────────────────────────────────────

void UYanSessionManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 依赖 UCommonSessionSubsystem 先完成初始化
	Collection.InitializeDependency<UCommonSessionSubsystem>();

	if (UCommonSessionSubsystem* SessionSub = GetGameInstance()->GetSubsystem<UCommonSessionSubsystem>())
	{
		// 绑定 Native 事件（不需要 UFUNCTION，直接 AddUObject）
		SessionSub->OnCreateSessionCompleteEvent.AddUObject(this, &UYanSessionManager::HandleCreateComplete);
		SessionSub->OnJoinSessionCompleteEvent.AddUObject(this, &UYanSessionManager::HandleJoinComplete);
		SessionSub->OnSessionInformationChangedEvent.AddUObject(this, &UYanSessionManager::HandleSessionInformationChanged);

		// 平台邀请的唯一入口。缺少这条绑定时，好友接受邀请后游戏侧不会有任何反应
		SessionSub->OnUserRequestedSessionEvent.AddUObject(this, &UYanSessionManager::HandleUserRequestedSession);
	}
}

void UYanSessionManager::Deinitialize()
{
	if (UCommonSessionSubsystem* SessionSub = GetGameInstance()->GetSubsystem<UCommonSessionSubsystem>())
	{
		SessionSub->OnCreateSessionCompleteEvent.RemoveAll(this);
		SessionSub->OnJoinSessionCompleteEvent.RemoveAll(this);
		SessionSub->OnSessionInformationChangedEvent.RemoveAll(this);
		SessionSub->OnUserRequestedSessionEvent.RemoveAll(this);
	}

	PendingSearchRequest = nullptr;
	CachedResults.Empty();

	Super::Deinitialize();
}

// ── Lobby 信息（UI 便捷展示） ────────────────────────────────────────────────

FText UYanSessionManager::GetLobbyInfoText() const
{
	const UWorld* World = GetWorld();
	const ENetMode NetMode = World ? World->GetNetMode() : NM_MAX;

	int32 PlayerCount = 0;
	if (World && World->GetGameState())
	{
		PlayerCount = World->GetGameState()->PlayerArray.Num();
	}

	auto NetModeToString = [](ENetMode InNetMode) -> const TCHAR*
	{
		switch (InNetMode)
		{
		case NM_Standalone: return TEXT("Standalone");
		case NM_DedicatedServer: return TEXT("DedicatedServer");
		case NM_ListenServer: return TEXT("ListenServer");
		case NM_Client: return TEXT("Client");
		default: return TEXT("Unknown");
		}
	};

	auto StateToString = [](ECommonSessionInformationState InState) -> const TCHAR*
	{
		switch (InState)
		{
		case ECommonSessionInformationState::OutOfGame: return TEXT("OutOfGame");
		case ECommonSessionInformationState::Matchmaking: return TEXT("Matchmaking");
		case ECommonSessionInformationState::InGame: return TEXT("InGame");
		default: return TEXT("Unknown");
		}
	};

	const FString OpLine = CachedLastSessionOp.IsEmpty()
		? FString()
		: FString::Printf(TEXT("Op: %s (%s)\n"), *CachedLastSessionOp, bLastSessionOpSucceeded ? TEXT("Success") : TEXT("Fail"));

	const FString OpMsgLine = CachedLastSessionOpMessage.IsEmpty()
		? FString()
		: FString::Printf(TEXT("OpMsg: %s\n"), *CachedLastSessionOpMessage.ToString());

	const FString Text = FString::Printf(
		TEXT("%s%sState: %s\nNetMode: %s\nMap: %s\nGameMode: %s\nPlayers: %d"),
		*OpLine,
		*OpMsgLine,
		StateToString(CachedSessionState),
		NetModeToString(NetMode),
		CachedMapName.IsEmpty() ? TEXT("N/A") : *CachedMapName,
		CachedGameMode.IsEmpty() ? TEXT("N/A") : *CachedGameMode,
		PlayerCount
	);

	return FText::FromString(Text);
}

// ── HostSession ───────────────────────────────────────────────────────────────

void UYanSessionManager::HostSession(APlayerController* HostingPlayer, UModularUserFacingExperienceDefinition* ExperienceDef)
{
	if (!HostingPlayer || !ExperienceDef)
	{
		OnSessionCreated.Broadcast(false, FText::FromString(TEXT("参数无效：HostingPlayer 或 ExperienceDef 为空")));
		return;
	}

	UCommonSessionSubsystem* SessionSub = GetGameInstance()->GetSubsystem<UCommonSessionSubsystem>();
	if (!SessionSub)
	{
		OnSessionCreated.Broadcast(false, FText::FromString(TEXT("UCommonSessionSubsystem 不可用")));
		return;
	}

	// 通过 ExperienceDef 构建请求（地图、MaxPlayer、ExtraArgs 等已由数据资产配置）
	UCommonSession_HostSessionRequest* Request = ExperienceDef->CreateHostingRequest(GetGameInstance());
	if (!Request)
	{
		OnSessionCreated.Broadcast(false, FText::FromString(TEXT("CreateHostingRequest 返回空")));
		return;
	}

	// 确保使用 Steam 联机模式
	Request->OnlineMode = ECommonSessionOnlineMode::Online;
	Request->bUseLobbies = true;
	Request->bUsePresence = true;

	SessionSub->HostSession(HostingPlayer, Request);
}

void UYanSessionManager::HostViaLobby(APlayerController* HostingPlayer, UCommonSession_HostSessionRequest* MatchRequest)
{
	UCommonSessionSubsystem* SessionSub = GetGameInstance()->GetSubsystem<UCommonSessionSubsystem>();
	if (!SessionSub)
	{
		OnSessionCreated.Broadcast(false, FText::FromString(TEXT("UCommonSessionSubsystem 不可用")));
		return;
	}

	PendingMatchRequest = nullptr;
	MatchRoster.Reset();

	// 以下任何分支都经 SessionSub 开房：调用方（Lyra 前端蓝图）依赖其完成事件收起等待界面，含失败
	if (!MatchRequest || MatchRequest->OnlineMode == ECommonSessionOnlineMode::Offline)
	{
		SessionSub->HostSession(HostingPlayer, MatchRequest);
		return;
	}

	const UModularUserFacingExperienceDefinition* LobbyExperience = GetDefault<UYanSessionSettings>()->LobbyExperience.LoadSynchronous();
	if (!LobbyExperience)
	{
		UE_LOG(LogYanSession, Warning, TEXT("未配置等待大厅（Yan 会话设置 > LobbyExperience），开房直接进入对局。"));
		SessionSub->HostSession(HostingPlayer, MatchRequest);
		return;
	}

	// 会话层参数沿用对局请求，使大厅阶段就按对局的人数上限与联机方式开放；
	// 广播的模式名也沿用对局，浏览列表与客机加载屏展示的是将要进行的对局
	UCommonSession_HostSessionRequest* LobbyRequest = LobbyExperience->CreateHostingRequest(this);
	LobbyRequest->OnlineMode = MatchRequest->OnlineMode;
	LobbyRequest->bUseLobbies = MatchRequest->bUseLobbies;
	LobbyRequest->bUseLobbiesVoiceChat = MatchRequest->bUseLobbiesVoiceChat;
	LobbyRequest->bUsePresence = MatchRequest->bUsePresence;
	LobbyRequest->MaxPlayerCount = MatchRequest->MaxPlayerCount;
	LobbyRequest->ModeNameForAdvertisement = MatchRequest->ModeNameForAdvertisement;

	// 对外广播的是大厅会话，可见性须随对局请求走，否则私有设置在大厅阶段失效
	if (const FString* const PrivateValue = MatchRequest->CustomSettings.Find(YanSessionKeys::Private))
	{
		LobbyRequest->CustomSettings.Add(YanSessionKeys::Private, *PrivateValue);
	}

	PendingMatchRequest = MatchRequest;

	SessionSub->HostSession(HostingPlayer, LobbyRequest);
}

// ── FindSessions ──────────────────────────────────────────────────────────────

void UYanSessionManager::FindSessions(APlayerController* SearchingPlayer)
{
	if (!SearchingPlayer)
	{
		OnSearchCompleted.Broadcast(false, FText::FromString(TEXT("参数无效：SearchingPlayer 为空")));
		return;
	}

	PendingRoomCode.Reset();
	PendingRoomCodeJoiner.Reset();

	StartSearch(SearchingPlayer, {}, /*bIncludePrivate=*/false);
}

void UYanSessionManager::StartSearch(APlayerController* SearchingPlayer, const TMap<FName, FString>& Filters, bool bIncludePrivate)
{
	UCommonSessionSubsystem* SessionSub = GetGameInstance()->GetSubsystem<UCommonSessionSubsystem>();
	if (!SessionSub)
	{
		HandleSearchFinished(false, FText::FromString(TEXT("UCommonSessionSubsystem 不可用")));
		return;
	}

	// 清空上次结果
	CachedResults.Empty();
	PendingSearchRequest = nullptr;

	UCommonSession_SearchSessionRequest* SearchRequest = SessionSub->CreateOnlineSearchSessionRequest();
	SearchRequest->OnlineMode = ECommonSessionOnlineMode::Online;
	SearchRequest->bUseLobbies = true;
	SearchRequest->CustomFilters.Append(Filters);
	if (bIncludePrivate)
	{
		SearchRequest->CustomFilters.Remove(YanSessionKeys::Private);
	}

	// 绑定搜索完成回调（Native，不需要 UFUNCTION）
	SearchRequest->OnSearchFinished.AddUObject(this, &UYanSessionManager::HandleSearchFinished);

	PendingSearchRequest = SearchRequest;
	SessionSub->FindSessions(SearchingPlayer, SearchRequest);
}

// ── 大厅选队名单 ──────────────────────────────────────────────────────────────

void UYanSessionManager::SetMatchRoster(TMap<FString, int32>&& InRoster, const FString& InMapPackageName)
{
	MatchRoster = MoveTemp(InRoster);
	MatchRosterMap = InMapPackageName;
}

bool UYanSessionManager::FindRosterTeamId(const APlayerState* PlayerState, int32& OutTeamId)
{
	OutTeamId = INDEX_NONE;

	const UWorld* const World = PlayerState ? PlayerState->GetWorld() : nullptr;
	const UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	const UYanSessionManager* const SessionManager = GameInstance ? GameInstance->GetSubsystem<UYanSessionManager>() : nullptr;
	if (!SessionManager || !SessionManager->MatchRoster.IsSet())
	{
		return false;
	}

	// 名单只对大厅开往的那张对局关卡生效，之后同一会话内的其他切图（如回到大厅）不再套用
	if (UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) != SessionManager->MatchRosterMap)
	{
		return false;
	}

	if (const int32* const TeamId = SessionManager->MatchRoster->Find(GetRosterKey(PlayerState)))
	{
		OutTeamId = *TeamId;
	}

	return true;
}

void UYanSessionManager::CarryMatchRosterTo(const UObject* WorldContextObject, const FString& NextMapPackageName)
{
	const UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	const UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	UYanSessionManager* const SessionManager = GameInstance ? GameInstance->GetSubsystem<UYanSessionManager>() : nullptr;
	if (!SessionManager || !SessionManager->MatchRoster.IsSet())
	{
		return;
	}

	if (UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == SessionManager->MatchRosterMap)
	{
		SessionManager->MatchRosterMap = NextMapPackageName;
	}
}

FString UYanSessionManager::GetRosterKey(const APlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return FString();
	}

	const FUniqueNetIdRepl& NetId = PlayerState->GetUniqueId();

	return NetId.IsValid() ? NetId.ToString() : PlayerState->GetPlayerName();
}

// ── 房间码 ────────────────────────────────────────────────────────────────────

void UYanSessionManager::JoinSessionByRoomCode(APlayerController* JoiningPlayer, const FString& RoomCode)
{
	if (!JoiningPlayer)
	{
		OnSessionJoined.Broadcast(false, FText::FromString(TEXT("参数无效：JoiningPlayer 为空")));
		return;
	}

	const FString Code = NormalizeRoomCode(RoomCode);
	UE_LOG(LogYanSession, Log, TEXT("[RoomCode] 按房间码加入：输入 [%s]，规范化后 [%s]"), *RoomCode, *Code);

	if (Code.IsEmpty())
	{
		OnSessionJoined.Broadcast(false, FText::FromString(FString::Printf(TEXT("房间码格式不正确，应为 %d 位字母或数字"), YanSessionKeys::RoomCodeLength)));
		return;
	}

	PendingRoomCode = Code;
	PendingRoomCodeJoiner = JoiningPlayer;

	TMap<FName, FString> Filters;
	Filters.Add(YanSessionKeys::RoomCode, Code);
	StartSearch(JoiningPlayer, Filters, /*bIncludePrivate=*/true);
}

FString UYanSessionManager::GetCurrentRoomCode() const
{
	IOnlineSubsystem* const OnlineSub = Online::GetSubsystem(GetWorld());
	const IOnlineSessionPtr Sessions = OnlineSub ? OnlineSub->GetSessionInterface() : nullptr;
	if (!Sessions.IsValid())
	{
		return FString();
	}

	// 客机加入时会把搜索结果里的设置复制进命名会话，因此两端读到的是同一份数据
	FString Code;
	if (const FNamedOnlineSession* const Session = Sessions->GetNamedSession(NAME_GameSession))
	{
		Session->SessionSettings.Get<FString>(YanSessionKeys::RoomCode, Code);
	}

	return Code;
}

FString UYanSessionManager::NormalizeRoomCode(const FString& Input)
{
	FString Code;
	Code.Reserve(YanSessionKeys::RoomCodeLength);

	for (const TCHAR Char : Input)
	{
		if (FChar::IsWhitespace(Char) || Char == TEXT('-'))
		{
			continue;
		}

		Code.AppendChar(FChar::ToUpper(Char));
	}

	if (Code.Len() != YanSessionKeys::RoomCodeLength)
	{
		return FString();
	}

	for (const TCHAR Char : Code)
	{
		int32 UnusedIndex = INDEX_NONE;
		if (!YanSessionKeys::RoomCodeAlphabet.FindChar(Char, UnusedIndex))
		{
			return FString();
		}
	}

	return Code;
}

void UYanSessionManager::SetHostRequestPrivate(UCommonSession_HostSessionRequest* Request, bool bPrivate)
{
	if (!Request)
	{
		return;
	}

	Request->CustomSettings.Add(YanSessionKeys::Private, bPrivate ? YanSessionKeys::PrivateTrue : YanSessionKeys::PrivateFalse);
}

bool UYanSessionManager::IsCurrentSessionPrivate() const
{
	IOnlineSubsystem* const OnlineSub = Online::GetSubsystem(GetWorld());
	const IOnlineSessionPtr Sessions = OnlineSub ? OnlineSub->GetSessionInterface() : nullptr;
	if (!Sessions.IsValid())
	{
		return false;
	}

	FString Value;
	if (const FNamedOnlineSession* const Session = Sessions->GetNamedSession(NAME_GameSession))
	{
		Session->SessionSettings.Get<FString>(YanSessionKeys::Private, Value);
	}

	return Value == YanSessionKeys::PrivateTrue;
}

int32 UYanSessionManager::GetCurrentMaxPlayers() const
{
	IOnlineSubsystem* const OnlineSub = Online::GetSubsystem(GetWorld());
	const IOnlineSessionPtr Sessions = OnlineSub ? OnlineSub->GetSessionInterface() : nullptr;
	if (!Sessions.IsValid())
	{
		return 0;
	}

	const FNamedOnlineSession* const Session = Sessions->GetNamedSession(NAME_GameSession);

	return Session ? Session->SessionSettings.NumPublicConnections + Session->SessionSettings.NumPrivateConnections : 0;
}

// ── JoinSession ───────────────────────────────────────────────────────────────

void UYanSessionManager::JoinSession(APlayerController* JoiningPlayer, UCommonSession_SearchResult* Result)
{
	if (!JoiningPlayer || !Result)
	{
		OnSessionJoined.Broadcast(false, FText::FromString(TEXT("参数无效：JoiningPlayer 或 Result 为空")));
		return;
	}

	UCommonSessionSubsystem* SessionSub = GetGameInstance()->GetSubsystem<UCommonSessionSubsystem>();
	if (!SessionSub)
	{
		OnSessionJoined.Broadcast(false, FText::FromString(TEXT("UCommonSessionSubsystem 不可用")));
		return;
	}

	PrepareLoadingScreenForSearchResult(Result);

	SessionSub->JoinSession(JoiningPlayer, Result);
}

void UYanSessionManager::JoinSessionByData(APlayerController* JoiningPlayer, UYanSessionSearchResultData* Data)
{
	if (!Data || !Data->SearchResult)
	{
		OnSessionJoined.Broadcast(false, FText::FromString(TEXT("无效的 SessionSearchResultData")));
		return;
	}

	JoinSession(JoiningPlayer, Data->SearchResult);
}

// ── CleanUpSessions ───────────────────────────────────────────────────────────

void UYanSessionManager::CleanUpSessions()
{
	ResetLocalSessionState();

	if (UCommonSessionSubsystem* SessionSub = GetGameInstance()->GetSubsystem<UCommonSessionSubsystem>())
	{
		SessionSub->CleanUpSessions();
	}
}

void UYanSessionManager::ReturnToMainMenu()
{
	ResetLocalSessionState();

	// UCommonGameInstance 会先清理 CommonSession 的会话，再断开连接并以 ?closed 回到默认地图
	GetGameInstance()->ReturnToMainMenu();
}

void UYanSessionManager::ResetLocalSessionState()
{
	CachedResults.Empty();
	PendingSearchRequest = nullptr;
	PendingRoomCode.Reset();
	PendingRoomCodeJoiner.Reset();
	PendingMatchRequest = nullptr;
	MatchRoster.Reset();
}

// ── 平台邀请 ──────────────────────────────────────────────────────────────────

bool UYanSessionManager::ShowInviteUI(APlayerController* InvitingPlayer)
{
	if (!InvitingPlayer)
	{
		return false;
	}

	const ULocalPlayer* LocalPlayer = InvitingPlayer->GetLocalPlayer();
	if (!LocalPlayer)
	{
		return false;
	}

	IOnlineSubsystem* const OnlineSub = Online::GetSubsystem(GetWorld());
	if (!OnlineSub)
	{
		return false;
	}

	const IOnlineExternalUIPtr ExternalUI = OnlineSub->GetExternalUIInterface();
	if (!ExternalUI.IsValid())
	{
		return false;
	}

	// 覆盖层面板按会话名定位要邀请进入的房间，NAME_GameSession 与 CommonSession 创建时一致
	return ExternalUI->ShowInviteUI(LocalPlayer->GetControllerId(), NAME_GameSession);
}

bool UYanSessionManager::CanInvite() const
{
	IOnlineSubsystem* const OnlineSub = Online::GetSubsystem(GetWorld());
	if (!OnlineSub)
	{
		return false;
	}

	const IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
	if (!Sessions.IsValid())
	{
		return false;
	}

	// 会话尚未建立时覆盖层无从知晓邀请目标，此时按钮应保持禁用
	const FNamedOnlineSession* const Session = Sessions->GetNamedSession(NAME_GameSession);

	return Session != nullptr && Session->SessionInfo.IsValid();
}

// ── 内部回调 ──────────────────────────────────────────────────────────────────

void UYanSessionManager::HandleCreateComplete(const FOnlineResultInformation& Result)
{
	CachedLastSessionOp = TEXT("Host");
	bLastSessionOpSucceeded = Result.bWasSuccessful;
	CachedLastSessionOpMessage = Result.bWasSuccessful
		? FText::FromString(TEXT("房间已创建，正在进入游戏…"))
		: Result.ErrorText;

	UE_LOG(LogYanSession, Log, TEXT("[Host] 开房结果：%s，房间码 [%s]，错误 [%s] %s"),
		Result.bWasSuccessful ? TEXT("成功") : TEXT("失败"), *GetCurrentRoomCode(), *Result.ErrorId, *Result.ErrorText.ToString());

	if (Result.bWasSuccessful)
	{
		OnSessionCreated.Broadcast(true, FText::FromString(TEXT("房间已创建，正在进入游戏…")));
	}
	else
	{
		PendingMatchRequest = nullptr;
		OnSessionCreated.Broadcast(false, Result.ErrorText);
	}
}

void UYanSessionManager::HandleJoinComplete(const FOnlineResultInformation& Result)
{
	CachedLastSessionOp = TEXT("Join");
	bLastSessionOpSucceeded = Result.bWasSuccessful;
	// Beacon 预约失败等路径只填 ErrorId 不填文本，界面会表现为无响应或空白弹窗
	const FText FailureText = Result.ErrorText.IsEmpty()
		? FText::FromString(FString::Printf(TEXT("加入失败（%s）"), Result.ErrorId.IsEmpty() ? TEXT("未知原因") : *Result.ErrorId))
		: Result.ErrorText;

	CachedLastSessionOpMessage = Result.bWasSuccessful ? FText::FromString(TEXT("加入成功，正在进入游戏…")) : FailureText;

	// 成功只代表加入了平台 Lobby，之后的 P2P 连接失败见 LogNet / TravelLocalSessionFailure
	UE_LOG(LogYanSession, Log, TEXT("[Join] 加入结果：%s，错误 [%s] %s"),
		Result.bWasSuccessful ? TEXT("成功") : TEXT("失败"), *Result.ErrorId, *Result.ErrorText.ToString());

	if (Result.bWasSuccessful)
	{
		OnSessionJoined.Broadcast(true, FText::FromString(TEXT("加入成功，正在进入游戏…")));
	}
	else
	{
		OnSessionJoined.Broadcast(false, FailureText);
	}
}

void UYanSessionManager::HandleSearchFinished(bool bSucceeded, const FText& ErrorMessage)
{
	CachedResults.Empty();

	if (!PendingRoomCode.IsEmpty())
	{
		FinishRoomCodeSearch(bSucceeded, ErrorMessage);
		return;
	}

	if (bSucceeded && PendingSearchRequest)
	{
		for (UCommonSession_SearchResult* RawResult : PendingSearchRequest->Results)
		{
			if (!RawResult)
			{
				continue;
			}

			// LAN 搜索不经平台过滤，私有房间须在本地剔除
			FString PrivateValue;
			bool bFoundPrivate = false;
			RawResult->GetStringSetting(YanSessionKeys::Private, PrivateValue, bFoundPrivate);
			if (bFoundPrivate && PrivateValue == YanSessionKeys::PrivateTrue)
			{
				continue;
			}

			UYanSessionSearchResultData* Data = NewObject<UYanSessionSearchResultData>(this);
			Data->SearchResult = RawResult;
			CachedResults.Add(Data); // UPROPERTY 数组，GC 会正确追踪裸指针
		}
	}

	const FText Message = bSucceeded ? FText::FromString(FString::Printf(TEXT("找到 %d 个房间"), CachedResults.Num())) : ErrorMessage;

	OnSearchCompleted.Broadcast(bSucceeded, Message);
}

void UYanSessionManager::FinishRoomCodeSearch(bool bSucceeded, const FText& ErrorMessage)
{
	const FString Code = PendingRoomCode;
	APlayerController* const JoiningPlayer = PendingRoomCodeJoiner.Get();
	PendingRoomCode.Reset();
	PendingRoomCodeJoiner.Reset();

	UE_LOG(LogYanSession, Log, TEXT("[RoomCode] 搜索房间码 [%s] 完成：%s，结果 %d 个，错误 [%s]"),
		*Code, bSucceeded ? TEXT("成功") : TEXT("失败"),
		PendingSearchRequest ? PendingSearchRequest->Results.Num() : -1, *ErrorMessage.ToString());

	if (!bSucceeded)
	{
		OnSessionJoined.Broadcast(false, ErrorMessage);
		return;
	}

	if (!JoiningPlayer)
	{
		OnSessionJoined.Broadcast(false, FText::FromString(TEXT("发起加入的玩家已失效")));
		return;
	}

	// LAN 搜索不做平台侧过滤，且共用测试 AppID 时可能混入他人房间，故本地按码再筛一次
	UCommonSession_SearchResult* Matched = nullptr;
	if (PendingSearchRequest)
	{
		for (UCommonSession_SearchResult* const Result : PendingSearchRequest->Results)
		{
			FString ResultCode;
			bool bFound = false;
			if (Result)
			{
				Result->GetStringSetting(YanSessionKeys::RoomCode, ResultCode, bFound);

				FString ResultProject;
				bool bFoundProject = false;
				Result->GetStringSetting(YanSessionKeys::Project, ResultProject, bFoundProject);

				UE_LOG(LogYanSession, Log, TEXT("[RoomCode] \t候选：%s，项目 [%s]，房间码 [%s]，空位 %d，Ping %d ms"),
					*Result->GetDescription(), *ResultProject, *ResultCode,
					Result->GetNumOpenPublicConnections(), Result->GetPingInMs());
			}

			if (bFound && ResultCode == Code)
			{
				Matched = Result;
				break;
			}
		}
	}

	if (!Matched)
	{
		UE_LOG(LogYanSession, Warning, TEXT("[RoomCode] 没有候选匹配房间码 [%s]"), *Code);
		OnSessionJoined.Broadcast(false, FText::FromString(TEXT("没有找到该房间码对应的房间，请确认房间仍在开放")));
		return;
	}

	JoinSession(JoiningPlayer, Matched);
}

void UYanSessionManager::HandleSessionInformationChanged(ECommonSessionInformationState SessionStatus, const FString& GameMode, const FString& MapName)
{
	CachedSessionState = SessionStatus;
	CachedGameMode = GameMode;
	CachedMapName = MapName;
}

void UYanSessionManager::HandleUserRequestedSession(const FPlatformUserId& PlatformUserId, UCommonSession_SearchResult* RequestedSession, const FOnlineResultInformation& RequestedSessionResult)
{
	if (!RequestedSessionResult.bWasSuccessful || !RequestedSession)
	{
		OnInviteAccepted.Broadcast(false, RequestedSessionResult.ErrorText.IsEmpty()
			? FText::FromString(TEXT("邀请无效或房间已不存在"))
			: RequestedSessionResult.ErrorText);
		return;
	}

	APlayerController* const JoiningPlayer = FindLocalPlayerController(PlatformUserId);
	if (!JoiningPlayer)
	{
		OnInviteAccepted.Broadcast(false, FText::FromString(TEXT("无法定位接受邀请的本地玩家")));
		return;
	}

	// JoinSession 会占用 NAME_GameSession，本机已在房间内时必然失败。
	// 销毁旧会话是异步的，无法在此立即完成，故交由玩家先行退出而不是静默失败。
	if (CanInvite())
	{
		OnInviteAccepted.Broadcast(false, FText::FromString(TEXT("已在房间中，请先退出当前房间再接受邀请")));
		return;
	}

	OnInviteAccepted.Broadcast(true, FText::FromString(TEXT("正在加入好友的房间…")));

	JoinSession(JoiningPlayer, RequestedSession);
}

void UYanSessionManager::PrepareLoadingScreenForSearchResult(UCommonSession_SearchResult* Result) const
{
	UYanLoadingScreenSubsystem* LoadingScreenSub = GetGameInstance()->GetSubsystem<UYanLoadingScreenSubsystem>();
	if (!LoadingScreenSub || !Result)
	{
		return;
	}

	// HostSession 时框架已把 ModeNameForAdvertisement（即 Experience 资产名）自动写入该键，
	// 详见 UCommonSessionSubsystem::HostSession 内对 SETTING_GAMEMODE 的写入
	FString AdvertisedExperienceName;
	bool bFound = false;
	Result->GetStringSetting(SETTING_GAMEMODE, AdvertisedExperienceName, bFound);
	if (!bFound || AdvertisedExperienceName.IsEmpty())
	{
		return;
	}

	const FPrimaryAssetId ExperienceId = UModularSystemStatics::GetPrimaryAssetIdFromUserFacingExperienceName(AdvertisedExperienceName);
	const TSoftObjectPtr<UObject> SoftExperience = UModularSystemStatics::GetTypedSoftObjectReferenceFromPrimaryAssetId(ExperienceId, UModularUserFacingExperienceDefinition::StaticClass());
	if (SoftExperience.IsNull())
	{
		return;
	}

	UAssetManager::GetStreamableManager().RequestAsyncLoad(
		SoftExperience.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(LoadingScreenSub, [LoadingScreenSub, SoftExperience]
		{
			if (const UModularUserFacingExperienceDefinition* ExperienceDef = Cast<UModularUserFacingExperienceDefinition>(SoftExperience.Get()))
			{
				LoadingScreenSub->PrepareLoadingScreenForExperience(ExperienceDef->LoadingScreenWidget);
			}
		}));
}

APlayerController* UYanSessionManager::FindLocalPlayerController(const FPlatformUserId& PlatformUserId) const
{
	const UGameInstance* const GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	for (ULocalPlayer* const LocalPlayer : GameInstance->GetLocalPlayers())
	{
		if (LocalPlayer && LocalPlayer->GetPlatformUserId() == PlatformUserId)
		{
			return LocalPlayer->GetPlayerController(GetWorld());
		}
	}

	// 邀请可能在玩家完成平台登录前到达，此时回退到首个本地玩家而非放弃加入
	return GameInstance->GetFirstLocalPlayerController();
}
