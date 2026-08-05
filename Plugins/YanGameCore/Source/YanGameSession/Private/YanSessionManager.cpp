// Copyright YanLao.

#include "YanSessionManager.h"
#include "GameMode/ModularUserFacingExperienceDefinition.h"
#include "CommonUserTypes.h"
#include "GameFramework/GameStateBase.h"

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
	}
}

void UYanSessionManager::Deinitialize()
{
	if (UCommonSessionSubsystem* SessionSub = GetGameInstance()->GetSubsystem<UCommonSessionSubsystem>())
	{
		SessionSub->OnCreateSessionCompleteEvent.RemoveAll(this);
		SessionSub->OnJoinSessionCompleteEvent.RemoveAll(this);
		SessionSub->OnSessionInformationChangedEvent.RemoveAll(this);
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

// ── FindSessions ──────────────────────────────────────────────────────────────

void UYanSessionManager::FindSessions(APlayerController* SearchingPlayer)
{
	if (!SearchingPlayer)
	{
		OnSearchCompleted.Broadcast(false, FText::FromString(TEXT("参数无效：SearchingPlayer 为空")));
		return;
	}

	UCommonSessionSubsystem* SessionSub = GetGameInstance()->GetSubsystem<UCommonSessionSubsystem>();
	if (!SessionSub)
	{
		OnSearchCompleted.Broadcast(false, FText::FromString(TEXT("UCommonSessionSubsystem 不可用")));
		return;
	}

	// 清空上次结果
	CachedResults.Empty();
	PendingSearchRequest = nullptr;

	UCommonSession_SearchSessionRequest* SearchRequest = SessionSub->CreateOnlineSearchSessionRequest();
	SearchRequest->OnlineMode = ECommonSessionOnlineMode::Online;
	SearchRequest->bUseLobbies = true;

	// 绑定搜索完成回调（Native，不需要 UFUNCTION）
	SearchRequest->OnSearchFinished.AddUObject(this, &UYanSessionManager::HandleSearchFinished);

	PendingSearchRequest = SearchRequest;
	SessionSub->FindSessions(SearchingPlayer, SearchRequest);
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
	CachedResults.Empty();
	PendingSearchRequest = nullptr;

	if (UCommonSessionSubsystem* SessionSub = GetGameInstance()->GetSubsystem<UCommonSessionSubsystem>())
	{
		SessionSub->CleanUpSessions();
	}
}

// ── 内部回调 ──────────────────────────────────────────────────────────────────

void UYanSessionManager::HandleCreateComplete(const FOnlineResultInformation& Result)
{
	CachedLastSessionOp = TEXT("Host");
	bLastSessionOpSucceeded = Result.bWasSuccessful;
	CachedLastSessionOpMessage = Result.bWasSuccessful
		? FText::FromString(TEXT("房间已创建，正在进入游戏…"))
		: Result.ErrorText;

	if (Result.bWasSuccessful)
	{
		OnSessionCreated.Broadcast(true, FText::FromString(TEXT("房间已创建，正在进入游戏…")));
	}
	else
	{
		OnSessionCreated.Broadcast(false, Result.ErrorText);
	}
}

void UYanSessionManager::HandleJoinComplete(const FOnlineResultInformation& Result)
{
	CachedLastSessionOp = TEXT("Join");
	bLastSessionOpSucceeded = Result.bWasSuccessful;
	CachedLastSessionOpMessage = Result.bWasSuccessful ? FText::FromString(TEXT("加入成功，正在进入游戏…")) : Result.ErrorText;

	if (Result.bWasSuccessful)
	{
		OnSessionJoined.Broadcast(true, FText::FromString(TEXT("加入成功，正在进入游戏…")));
	}
	else
	{
		OnSessionJoined.Broadcast(false, Result.ErrorText);
	}
}

void UYanSessionManager::HandleSearchFinished(bool bSucceeded, const FText& ErrorMessage)
{
	CachedResults.Empty();

	if (bSucceeded && PendingSearchRequest)
	{
		for (UCommonSession_SearchResult* RawResult : PendingSearchRequest->Results)
		{
			if (!RawResult)
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

void UYanSessionManager::HandleSessionInformationChanged(ECommonSessionInformationState SessionStatus, const FString& GameMode, const FString& MapName)
{
	CachedSessionState = SessionStatus;
	CachedGameMode = GameMode;
	CachedMapName = MapName;
}
