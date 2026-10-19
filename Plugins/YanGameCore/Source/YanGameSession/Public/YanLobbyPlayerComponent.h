// Copyright YanLao.

#pragma once

#include "Components/PlayerStateComponent.h"

#include "YanLobbyPlayerComponent.generated.h"

#define UE_API YANGAMESESSION_API

class UYanLobbyComponent;

/**
 * 玩家在等待大厅内的状态：所选队伍与是否准备。
 *
 * 由大厅 Experience 注入到 PlayerState。PlayerState 归属于玩家自己的控制器，
 * 因此选队、准备、聊天都经本组件的 Server RPC 发出，裁决统一交给服务器上的 UYanLobbyComponent。
 */
UCLASS(MinimalAPI, Blueprintable, Meta = (BlueprintSpawnableComponent))
class UYanLobbyPlayerComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	UE_API explicit UYanLobbyPlayerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 取 PlayerState 上的大厅状态，不在大厅中时为 nullptr */
	UFUNCTION(BlueprintPure, Category = "Yan|Lobby")
	static UE_API UYanLobbyPlayerComponent* FindLobbyPlayer(const APlayerState* PlayerState);

	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End UActorComponent Interface

	/** 请求加入队伍；INDEX_NONE 表示回到未分配。换队会取消准备，满员或队伍不存在时忽略 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Lobby")
	UE_API void RequestJoinTeam(int32 InTeamId);

	UFUNCTION(BlueprintCallable, Category = "Yan|Lobby")
	UE_API void RequestSetReady(bool bInReady);

	/** 发送一条大厅聊天；空白消息被忽略，过长的消息被截断 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Lobby")
	UE_API void SendChatMessage(const FString& Message);

	/** 所选队伍，INDEX_NONE 表示未分配（开局后观战） */
	UFUNCTION(BlueprintPure, Category = "Yan|Lobby")
	int32 GetTeamId() const { return TeamId; }

	UFUNCTION(BlueprintPure, Category = "Yan|Lobby")
	bool IsReady() const { return bReady; }

	/** 是否为房主（ListenServer 上的本地玩家） */
	UFUNCTION(BlueprintPure, Category = "Yan|Lobby")
	bool IsHost() const { return bIsHost; }

	// 仅由服务器上的 UYanLobbyComponent 在裁决通过后调用
	void AuthSetTeamId(int32 InTeamId);
	void AuthSetReady(bool bInReady);

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestJoinTeam(int32 InTeamId);

	UFUNCTION(Server, Reliable)
	void ServerRequestSetReady(bool bInReady);

	UFUNCTION(Server, Reliable)
	void ServerSendChatMessage(const FString& Message);

	UYanLobbyComponent* FindLobby() const;

	UPROPERTY(Replicated)
	int32 TeamId = INDEX_NONE;

	UPROPERTY(Replicated)
	bool bReady = false;

	UPROPERTY(Replicated)
	bool bIsHost = false;
};

#undef UE_API
