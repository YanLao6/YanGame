// Copyright YanLao.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CommonSessionSubsystem.h"          // UCommonSessionSubsystem, UCommonSession_SearchResult
#include "YanSessionSearchResultData.h"       // 同模块，供 CachedResults 使用
#include "YanSessionManager.generated.h"

#define UE_API YANGAMESESSION_API

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

	/** 搜索所有可见的 Steam 联机房间（Online + Lobby），结果存入 CachedResults */
	UFUNCTION(BlueprintCallable, Category = "Yan|Session")
	UE_API void FindSessions(APlayerController* SearchingPlayer);

	/** 加入指定的 SearchResult 对应的房间 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Session")
	UE_API void JoinSession(APlayerController* JoiningPlayer, UCommonSession_SearchResult* Result);

	/** AngelScript 便利重载：直接传入 CachedResults 中的条目 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Session")
	UE_API void JoinSessionByData(APlayerController* JoiningPlayer, UYanSessionSearchResultData* Data);

	/** 清理当前会话（例如返回主菜单时调用） */
	UFUNCTION(BlueprintCallable, Category = "Yan|Session")
	UE_API void CleanUpSessions();

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

private:
	// 内部回调 —— 绑定到 UCommonSessionSubsystem 的 Native 事件
	UE_API void HandleCreateComplete(const FOnlineResultInformation& Result);
	UE_API void HandleJoinComplete(const FOnlineResultInformation& Result);

	// 绑定到 UCommonSession_SearchSessionRequest::OnSearchFinished
	UE_API void HandleSearchFinished(bool bSucceeded, const FText& ErrorMessage);

	// 监听 CommonUser 的“可展示会话信息”变化（Map/GameMode/State）
	UE_API void HandleSessionInformationChanged(ECommonSessionInformationState SessionStatus, const FString& GameMode, const FString& MapName);

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
};

#undef UE_API
