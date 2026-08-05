#pragma once

#include "Components/GameStateComponent.h"

#include "HeroSelectionBoardComponent.generated.h"

#define UE_API HEROASSIGNMENTSYSTEM_API

class APlayerState;
class UModularUserFacingExperienceDefinition;

/** 单个玩家的选人进度。 */
USTRUCT(BlueprintType)
struct FHeroSelectionEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Hero")
	TObjectPtr<APlayerState> Player;

	UPROPERTY(BlueprintReadOnly, Category = "Hero")
	FPrimaryAssetId HeroId;

	UPROPERTY(BlueprintReadOnly, Category = "Hero")
	bool bReady = false;
};

/** 看板内容变化时广播，供选人界面刷新。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHeroSelectionBoardChanged);

/**
 * 大厅选人看板。
 *
 * 由大厅 Experience 注入到 GameState，是全场选人进度的唯一真值。
 * Controller 上的 UHeroSelectionComponent 只向拥有者复制，玩家据此无法得知队友的进度，
 * 而角色互斥与开局判定都需要全场视野，故另设本组件复制给所有客户端。
 *
 * 只接受服务器写入，客户端请求经 UHeroSelectionComponent 的 Server RPC 上报。
 */
UCLASS(MinimalAPI, Blueprintable)
class UHeroSelectionBoardComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UE_API explicit UHeroSelectionBoardComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Authority：登记玩家的选择。
	 *
	 * @return 是否被接受。角色已被他人选走，或该玩家已确认时为 false。
	 */
	UE_API bool SetSelection(APlayerState* Player, const FPrimaryAssetId& HeroId);

	/** Authority：设置玩家的确认状态，达成开局条件时发起切图。尚未选择角色时确认不生效。 */
	UE_API void SetReady(APlayerState* Player, bool bInReady);

	/** 玩家已登记的选择，未登记时返回无效 Id。 */
	UE_API FPrimaryAssetId GetSelectedHero(const APlayerState* Player) const;

	/** 该角色是否已被 Requester 之外的玩家选走。 */
	UE_API bool IsHeroTakenByOther(const FPrimaryAssetId& HeroId, const APlayerState* Requester) const;

	/** 玩家是否已确认选择。 */
	UFUNCTION(BlueprintPure, Category = "Hero")
	UE_API bool IsPlayerReady(APlayerState* Player) const;

	/** 已确认的玩家数。 */
	UFUNCTION(BlueprintPure, Category = "Hero")
	UE_API int32 GetReadyPlayerCount() const;

	/** 在场且参与选人的玩家数。 */
	UFUNCTION(BlueprintPure, Category = "Hero")
	UE_API int32 GetActivePlayerCount() const;

	/** 开局所需的玩家数。 */
	UFUNCTION(BlueprintPure, Category = "Hero")
	int32 GetRequiredPlayerCount() const { return RequiredPlayerCount; }

	UPROPERTY(BlueprintAssignable, Category = "Hero")
	FOnHeroSelectionBoardChanged OnBoardChanged;

	//~Begin UActorComponent Interface
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End UActorComponent Interface

protected:
	/** 全员确认后要进入的对局 Experience，未配置时不会开局。 */
	UPROPERTY(EditDefaultsOnly, Category = "Hero")
	TObjectPtr<UModularUserFacingExperienceDefinition> NextExperience;

	/** 开局所需的最少玩家数，人数不足时确认不会触发切图。 */
	UPROPERTY(EditDefaultsOnly, Category = "Hero", Meta = (ClampMin = "1"))
	int32 RequiredPlayerCount = 2;

private:
	UFUNCTION()
	UE_API void OnRep_Entries();

	UE_API FHeroSelectionEntry&       FindOrAddEntry(APlayerState* Player);
	UE_API const FHeroSelectionEntry* FindEntry(const APlayerState* Player) const;

	// 在场玩家均已选角并确认，且人数达到要求
	UE_API bool CanStartMatch() const;

	// 按 NextExperience 构造 URL 并带上所有客户端切图
	UE_API void StartMatchTravel();

	// 玩家离场后其 PlayerState 被回收，留下的空条目在此清除
	UE_API void CompactEntries();

	UPROPERTY(ReplicatedUsing = OnRep_Entries)
	TArray<FHeroSelectionEntry> Entries;

	// 切图只发起一次，Travel 期间到达的确认不再重复触发
	bool bMatchTravelStarted = false;
};

#undef UE_API
