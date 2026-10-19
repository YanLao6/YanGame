// Copyright YanLao.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CommonSessionSubsystem.h"          // UCommonSessionSubsystem, UCommonSession_SearchResult
#include "YanSessionSearchResultData.h"       // 同模块，供 CachedResults 使用
#include "YanSessionManager.generated.h"

#define UE_API YANGAMESESSION_API

class APlayerState;
class UModularUserFacingExperienceDefinition;
struct FOnlineResultInformation;

// ── Blueprint / AngelScript 可绑定的事件委托 ──────────────────────────────────

/** 托管房间完成；bSuccess=true 表示成功，Message 携带状态文本 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FYanOnSessionCreated, bool, bSuccess, FText, Message);

/** 搜索房间完成；bSuccess=true 时 CachedResults 已填充 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FYanOnSearchCompleted, bool, bSuccess, FText, Message);

/** 加入房间完成 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FYanOnSessionJoined, bool, bSuccess, FText, Message);

/**
 * 本机玩家接受了平台邀请（Steam 覆盖层点击“加入游戏”）。
 * bAutoJoining=true 表示已自动发起加入，后续结果由 OnSessionJoined 给出。
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FYanOnInviteAccepted, bool, bAutoJoining, FText, Message);

/**
 * UYanSessionManager
 *
 * GameInstance 级别的会话管理子系统。
 * 封装 UCommonSessionSubsystem 的 Host / Find / Join 三条主要流程，
 * 并使用 UModularUserFacingExperienceDefinition::CreateHostingRequest() 构建房间请求。
 *
 * AngelScript UI Widget 只需调用本类的接口，无需直接接触 FOnlineResultInformation。
 */
UCLASS(MinimalAPI)
class UYanSessionManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~UGameInstanceSubsystem interface
	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UE_API virtual void Deinitialize() override;
	//~End

	// ── Lobby 信息（UI 便捷展示） ─────────────────────────────────────────────

	/**
	 * 返回当前“大厅/会话”可展示信息（Host/Join 后进入地图时使用）。
	 * 说明：
	 * - 该文本用于 UI 展示，不作为“是否已联机成功”的权威判据（权威是 OnSessionCreated/OnSessionJoined）。
	 * - Map/GameMode/State 来自 UCommonSessionSubsystem::OnSessionInformationChangedEvent。
	 * - 玩家数来自当前 World->GameState->PlayerArray.Num()（进入大厅后最直观）。
	 */
	UFUNCTION(BlueprintPure, Category = "Yan|Session")
	UE_API FText GetLobbyInfoText() const;

	// ── 主要操作 ──────────────────────────────────────────────────────────────

	/**
	 * 使用 ExperienceDef 中的地图、人数等配置创建并托管 Steam 联机房间。
	 * 底层调用 UModularUserFacingExperienceDefinition::CreateHostingRequest()
	 * 再转发给 UCommonSessionSubsystem::HostSession()。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Session")
	UE_API void HostSession(APlayerController* HostingPlayer, UModularUserFacingExperienceDefinition* ExperienceDef);

	/**
	 * 联机开房时先进入等待大厅，MatchRequest 暂存为待开局请求，由大厅开局时再切往。
	 * 离线请求或未配置大厅（UYanSessionSettings::LobbyExperience）时直接按 MatchRequest 开房。
	 * 结果与 UCommonSessionSubsystem::HostSession 一致，经其 OnCreateSessionComplete 广播。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Session")
	UE_API void HostViaLobby(APlayerController* HostingPlayer, UCommonSession_HostSessionRequest* MatchRequest);

	/** 大厅开局时要切往的对局请求，仅在房主经 HostViaLobby 开房后存在 */
	const UCommonSession_HostSessionRequest* GetPendingMatchRequest() const { return PendingMatchRequest; }

	// ── 大厅选队名单（服务器） ────────────────────────────────────────────────

	/**
	 * 大厅开局时写入的选队结果，键为 GetRosterKey，值为队伍编号（INDEX_NONE 表示未分配）。
	 * 只对 InMapPackageName 指定的对局关卡生效。
	 */
	UE_API void SetMatchRoster(TMap<FString, int32>&& InRoster, const FString& InMapPackageName);

	/**
	 * 查询玩家在大厅中的选队，按玩家所在世界找到会话管理器。
	 * @return 当前关卡是否是大厅开往的对局；为 false 时 OutTeamId 无意义，应按对局自身规则分队。
	 *         为 true 时 OutTeamId 为 INDEX_NONE 表示未分配（含大厅开局后才加入的玩家）。
	 */
	static UE_API bool FindRosterTeamId(const APlayerState* PlayerState, int32& OutTeamId);

	/**
	 * 对局内换关（如闯关的下一关）前调用，让选队名单随之转到下一张关卡。
	 * 仅当当前关卡正是名单适用的关卡时生效，其余切图（如回到大厅）不受影响。
	 */
	static UE_API void CarryMatchRosterTo(const UObject* WorldContextObject, const FString& NextMapPackageName);

	/** 名单的键：跨切图保持不变的平台账号标识，取不到时退回玩家名 */
	static UE_API FString GetRosterKey(const APlayerState* PlayerState);

	/** 搜索所有可见的 Steam 联机房间（Online + Lobby），结果存入 CachedResults */
	UFUNCTION(BlueprintCallable, Category = "Yan|Session")
	UE_API void FindSessions(APlayerController* SearchingPlayer);

	/** 加入指定的 SearchResult 对应的房间 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Session")
	UE_API void JoinSession(APlayerController* JoiningPlayer, UCommonSession_SearchResult* Result);

	/** AngelScript 便利重载：直接传入 CachedResults 中的条目 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Session")
	UE_API void JoinSessionByData(APlayerController* JoiningPlayer, UYanSessionSearchResultData* Data);

	// ── 房间码 ────────────────────────────────────────────────────────────────

	/**
	 * 按房间码定位并加入房间，玩家之间无需好友关系。
	 * 码由 HostSession 生成并作为 Lobby 数据公开；搜索经平台按码过滤后，本地再比对一次才加入。
	 * 结果统一经 OnSessionJoined 广播：格式不合法、房间不存在均为 bSuccess=false。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Session|RoomCode")
	UE_API void JoinSessionByRoomCode(APlayerController* JoiningPlayer, const FString& RoomCode);

	/** 当前会话的房间码。房主与已加入的客机都能读到，不在房间中时为空 */
	UFUNCTION(BlueprintPure, Category = "Yan|Session|RoomCode")
	UE_API FString GetCurrentRoomCode() const;

	/** 把玩家输入整理成标准房间码（去空白与连字符、转大写）；长度或字符不合法时返回空串 */
	UFUNCTION(BlueprintPure, Category = "Yan|Session|RoomCode")
	static UE_API FString NormalizeRoomCode(const FString& Input);

	/**
	 * 设置开房请求是否为私有房间。
	 * 私有房间不出现在浏览列表与快速匹配中，只能凭房间码或平台邀请加入。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Session|RoomCode")
	static UE_API void SetHostRequestPrivate(UCommonSession_HostSessionRequest* Request, bool bPrivate);

	/** 当前会话是否为私有房间，不在房间中时为 false */
	UFUNCTION(BlueprintPure, Category = "Yan|Session|RoomCode")
	UE_API bool IsCurrentSessionPrivate() const;

	/** 当前会话的人数上限，不在房间中时为 0 */
	UFUNCTION(BlueprintPure, Category = "Yan|Session")
	UE_API int32 GetCurrentMaxPlayers() const;

	/** 清理当前会话（例如返回主菜单时调用） */
	UFUNCTION(BlueprintCallable, Category = "Yan|Session")
	UE_API void CleanUpSessions();

	/**
	 * 离开当前房间并回到前端（GameMapsSettings 的默认地图）。
	 * 房主离开时客机随连接断开一并回到前端。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Session")
	UE_API void ReturnToMainMenu();

	// ── 平台邀请 ──────────────────────────────────────────────────────────────

	/**
	 * 弹出平台好友邀请面板（Steam 覆盖层）。
	 *
	 * 前置条件：本机已建立或已加入会话，且平台覆盖层可用。
	 * 邀请只能由会话内的玩家发出，房主与已加入的客机都可调用。
	 *
	 * @return 面板是否成功弹出。false 时 CanInvite() 可用于判断原因。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Session|Invite")
	UE_API bool ShowInviteUI(APlayerController* InvitingPlayer);

	/** 当前是否处于可发出邀请的状态（存在有效会话且平台接口可用） */
	UFUNCTION(BlueprintPure, Category = "Yan|Session|Invite")
	UE_API bool CanInvite() const;

public:
	// ── 搜索结果 ──────────────────────────────────────────────────────────────

	/**
	 * 最近一次 FindSessions 成功后缓存的结果列表，可直接添加到 UListView。
	 * 由 OnSearchCompleted(true) 触发后再读取。
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Yan|Session")
	TArray<UYanSessionSearchResultData*> CachedResults;

	// ── 事件 ──────────────────────────────────────────────────────────────────

	/** 托管完成事件，在 HostSession 调用后由引擎回调触发 */
	UPROPERTY(BlueprintAssignable, Category = "Yan|Session|Events")
	FYanOnSessionCreated OnSessionCreated;

	/** 搜索完成事件，在 FindSessions 调用后由引擎回调触发 */
	UPROPERTY(BlueprintAssignable, Category = "Yan|Session|Events")
	FYanOnSearchCompleted OnSearchCompleted;

	/** 加入完成事件，在 JoinSession 调用后由引擎回调触发 */
	UPROPERTY(BlueprintAssignable, Category = "Yan|Session|Events")
	FYanOnSessionJoined OnSessionJoined;

	/** 接受平台邀请事件，由 Steam 覆盖层的“加入游戏”触发 */
	UPROPERTY(BlueprintAssignable, Category = "Yan|Session|Events")
	FYanOnInviteAccepted OnInviteAccepted;

private:
	// 内部回调 —— 绑定到 UCommonSessionSubsystem 的 Native 事件
	UE_API void HandleCreateComplete(const FOnlineResultInformation& Result);
	UE_API void HandleJoinComplete(const FOnlineResultInformation& Result);

	// 绑定到 UCommonSession_SearchSessionRequest::OnSearchFinished
	UE_API void HandleSearchFinished(bool bSucceeded, const FText& ErrorMessage);

	// 列表浏览与房间码加入共用的搜索入口；Filters 之外还会追加项目标识过滤，bIncludePrivate 为 false 时排除私有房间
	UE_API void StartSearch(APlayerController* SearchingPlayer, const TMap<FName, FString>& Filters, bool bIncludePrivate);

	// 房间码搜索完成后的分支：命中则加入，否则以 OnSessionJoined 报告失败
	UE_API void FinishRoomCodeSearch(bool bSucceeded, const FText& ErrorMessage);

	// 从房间广播的 GAMEMODE 反查 Experience 资产，异步取其 LoadingScreenWidget 推给加载屏子系统；
	// 反查或加载失败时静默跳过，不阻塞加入流程，由加载屏宿主 Widget 自身回退到默认内容
	UE_API void PrepareLoadingScreenForSearchResult(UCommonSession_SearchResult* Result) const;

	// 监听 CommonUser 的“可展示会话信息”变化（Map/GameMode/State）
	UE_API void HandleSessionInformationChanged(ECommonSessionInformationState SessionStatus, const FString& GameMode, const FString& MapName);

	// 本机玩家接受平台邀请（游戏运行中点击 Steam 覆盖层的“加入游戏”）
	UE_API void HandleUserRequestedSession(const FPlatformUserId& PlatformUserId, UCommonSession_SearchResult* RequestedSession, const FOnlineResultInformation& RequestedSessionResult);

	// 由 PlatformUserId 解析出对应的本地 PlayerController，找不到时回退到首个本地玩家
	UE_API APlayerController* FindLocalPlayerController(const FPlatformUserId& PlatformUserId) const;

	/** 最近一次 CommonSession 广播的可展示状态（主要用于 UI 文本） */
	ECommonSessionInformationState CachedSessionState = ECommonSessionInformationState::OutOfGame;

	/** 最近一次 CommonSession 广播的 ModeName */
	FString CachedGameMode;

	/** 最近一次 CommonSession 广播的 MapName（通常为包路径） */
	FString CachedMapName;

	/** 最近一次 Host/Join 结果（用于 UI 辅助展示） */
	bool bLastSessionOpSucceeded = false;
	FString CachedLastSessionOp;
	FText CachedLastSessionOpMessage;

	UPROPERTY(Transient)
	TObjectPtr<UCommonSession_SearchSessionRequest> PendingSearchRequest;

	UPROPERTY(Transient)
	TObjectPtr<UCommonSession_HostSessionRequest> PendingMatchRequest;

	/** 大厅开局时的选队结果，会话结束或重新开房时清空 */
	TOptional<TMap<FString, int32>> MatchRoster;

	/** 选队结果适用的对局关卡包名 */
	FString MatchRosterMap;

	// 清理本子系统自身的会话相关状态，不触及 CommonSession
	void ResetLocalSessionState();

	/** 非空表示当前搜索是房间码加入而非列表浏览，完成后自动加入命中的房间 */
	FString PendingRoomCode;

	/** 房间码搜索完成后要执行加入的玩家 */
	TWeakObjectPtr<APlayerController> PendingRoomCodeJoiner;
};

#undef UE_API
