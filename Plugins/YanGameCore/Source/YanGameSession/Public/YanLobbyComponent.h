// Copyright YanLao.

#pragma once

#include "Components/GameStateComponent.h"

#include "YanLobbyComponent.generated.h"

#define UE_API YANGAMESESSION_API

class APlayerState;
class UCommonSession_HostSessionRequest;
class UModularTeamDisplayAsset;
class UModularUserFacingExperienceDefinition;
class UYanLobbyPlayerComponent;

/** 一条大厅聊天 */
USTRUCT(BlueprintType)
struct FYanLobbyChatMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Yan|Lobby")
	FString SenderName;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|Lobby")
	FString Text;

	/** 单调递增的序号；列表有长度上限，界面据此判断哪些消息是新到的 */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Lobby")
	int32 Serial = 0;
};

/**
 * 等待大厅的规则与共享数据。
 *
 * 由大厅 Experience 注入到 GameState。玩家在大厅选队、准备、聊天，
 * 全员准备后自动开局，房主也可无视准备强制开局；开局时全部客机被 ServerTravel 一并带往对局关卡，
 * 大厅内的选队结果交给 UYanSessionManager 保存，由对局侧据此分队，未分配的玩家进入观战。
 *
 * 所有裁决都在服务器上进行：客户端的请求经 UYanLobbyPlayerComponent 的 Server RPC 到达，
 * 房主所在的 ListenServer 本身持有 Authority，可直接调用 RequestStartMatch。
 */
UCLASS(MinimalAPI, Blueprintable, Meta = (BlueprintSpawnableComponent))
class UYanLobbyComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UE_API explicit UYanLobbyComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End UActorComponent Interface

	/**
	 * 房主强制开局，无视准备状态。
	 *
	 * 仅在 Authority 上生效，且要求满足 CanStartMatch。
	 * 切图只发起一次，Travel 期间重复调用被忽略。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Lobby")
	UE_API void RequestStartMatch();

	/** 当前是否可以开局：有对局可去、人数达标、尚未开始切图；全员在观战席也可开局。只在服务器上有意义 */
	UFUNCTION(BlueprintPure, Category = "Yan|Lobby")
	UE_API bool CanStartMatch() const;

	/** 在场且非旁观的玩家数 */
	UFUNCTION(BlueprintPure, Category = "Yan|Lobby")
	UE_API int32 GetActivePlayerCount() const;

	/** 开局所需的最少玩家数 */
	UFUNCTION(BlueprintPure, Category = "Yan|Lobby")
	int32 GetMinPlayerCount() const { return MinPlayerCount; }

	/** 按编号升序排列的队伍 */
	UFUNCTION(BlueprintPure, Category = "Yan|Lobby")
	UE_API TArray<int32> GetTeamIds() const;

	/** 队伍的展示资产（名称、颜色），未配置时为 nullptr */
	UFUNCTION(BlueprintPure, Category = "Yan|Lobby")
	UE_API UModularTeamDisplayAsset* GetTeamDisplayAsset(int32 TeamId) const;

	/** 每支队伍的人数上限，0 表示不限 */
	UFUNCTION(BlueprintPure, Category = "Yan|Lobby")
	int32 GetTeamCapacity() const { return TeamCapacity; }

	/** 选择了指定队伍的在场玩家，按加入顺序排列；INDEX_NONE 取未分配的玩家 */
	UFUNCTION(BlueprintPure, Category = "Yan|Lobby")
	UE_API TArray<APlayerState*> GetPlayersInTeam(int32 TeamId) const;

	/** 最近的聊天记录，旧消息在前 */
	UFUNCTION(BlueprintPure, Category = "Yan|Lobby")
	const TArray<FYanLobbyChatMessage>& GetChatMessages() const { return ChatMessages; }

	// 以下仅由服务器上的 UYanLobbyPlayerComponent 转发调用
	void AuthJoinTeam(UYanLobbyPlayerComponent* Player, int32 TeamId);
	void AuthSetReady(UYanLobbyPlayerComponent* Player, bool bReady);
	void AuthAddChatMessage(const APlayerState* Sender, const FString& Message);

protected:
	/**
	 * 默认对局，仅在房主未经 UYanSessionManager::HostViaLobby 选定对局时使用（如编辑器直接打开大厅关卡）。
	 *
	 * 必须经由 UserFacingExperience 而非直接写地图路径：
	 * 对局关卡据以加载 Experience 的参数由 CreateHostingRequest 构造进 URL。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Lobby")
	TObjectPtr<UModularUserFacingExperienceDefinition> NextExperience;

	/** 开局所需的最少玩家数，人数不足时开局按钮不可用 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Lobby", Meta = (ClampMin = "1"))
	int32 MinPlayerCount = 1;

	/**
	 * 可选择的队伍：编号到展示资产。
	 * 编号须与对局 Experience 建队组件的 TeamsToCreate 一致，否则选队结果在对局中不生效。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Lobby")
	TMap<uint8, TObjectPtr<UModularTeamDisplayAsset>> Teams;

	/** 保留的聊天条数 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Lobby", Meta = (ClampMin = "1"))
	int32 MaxChatMessages = 50;

	/** 单条聊天的最大字符数 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Lobby", Meta = (ClampMin = "1"))
	int32 MaxChatMessageLength = 120;

private:
	// 全员准备且可开局时自动开局；任何可能改变条件的事件（准备、换队、离开）之后调用
	void EvaluateAutoStart();

	// 把选队结果交给 UYanSessionManager，再按对局请求构造 URL 并带上所有客户端切图
	void StartMatchTravel();

	bool IsValidTeamId(int32 TeamId) const;

	// 玩家离开后剩余玩家可能恰好全员准备
	void HandlePlayerLogout(class AGameModeBase* GameMode, AController* Exiting);

	/** 开局要切往的对局，仅在服务器上于 BeginPlay 确定 */
	UPROPERTY(Transient)
	TObjectPtr<const UCommonSession_HostSessionRequest> MatchRequest;

	UPROPERTY(Replicated)
	int32 TeamCapacity = 0;

	UPROPERTY(Replicated)
	TArray<FYanLobbyChatMessage> ChatMessages;

	int32 NextChatSerial = 1;

	FDelegateHandle LogoutHandle;

	// 切图只发起一次，Travel 期间到达的重复请求不再触发
	bool bMatchTravelStarted = false;
};

#undef UE_API
