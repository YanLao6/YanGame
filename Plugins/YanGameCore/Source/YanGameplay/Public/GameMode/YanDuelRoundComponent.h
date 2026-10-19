#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"

#include "YanDuelRoundComponent.generated.h"

#define UE_API YANGAMEPLAY_API

class APlayerState;
class UAbilitySystemComponent;

/** 计分变化时广播，供记分板刷新。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnYanDuelScoreChanged);

/** 整场分出胜负时广播，携带胜者（平局时为空）。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnYanDuelMatchFinished, APlayerState*, Winner);

/** 新回合开始时广播，供随回合复位的场地机关（如收缩环）重置自身。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnYanDuelRoundStarted);

/**
 * 单个玩家在本场比赛中的成绩。
 */
USTRUCT(BlueprintType)
struct FYanDuelPlayerScore
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	TObjectPtr<APlayerState> PlayerState = nullptr;

	/** 当前这一局已得的分数，赢下一局后清零 */
	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	int32 RoundPoints = 0;

	/** 已赢下的局数 */
	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	int32 SetsWon = 0;
};

/**
 * 对抗赛制：回合 → 局 → 场三级推进，并裁决每个回合归谁。
 *
 * 三级的划分来自赛制预算：一个回合是一次出界结算（上限 90 秒），先得 3 分赢一局，
 * 三局两胜为一场。回合上限的作用不是控制时长，而是压制龟缩——超时按负荷较低者判分，
 * 躲着不打的一方并不占便宜。
 *
 * 出界不由本组件探测。ADeathActor 判定出界后触发死亡技能，死亡技能再回头通知本组件，
 * 因为「谁出界了」这件事本就只有出界那一侧知道得最早、最准。本组件只负责它之后的裁决。
 *
 * 双方同时出界不计分，而非各得一分：后者会让「用自杀换分」成为一种可行打法，
 * 落后方总能靠同归于尽把比分拖平，追杀的风险因而消失。判定给一个短窗口，
 * 因为两次出界重叠事件本就不会落在同一帧。
 *
 * 全部状态只在服务器推进，分数复制给客户端只作展示。
 */
UCLASS(MinimalAPI, ClassGroup = (Yan), Meta = (BlueprintSpawnableComponent))
class UYanDuelRoundComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UE_API explicit UYanDuelRoundComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 出界通知，仅服务器有效。由死亡流程在判定出界后调用。 */
	UE_API void NotifyPlayerOut(APlayerState* OutPlayer);

	UFUNCTION(BlueprintPure, Category = "Duel")
	const TArray<FYanDuelPlayerScore>& GetScores() const { return Scores; }

	/** 本回合剩余秒数；回合未进行时为 0。 */
	UFUNCTION(BlueprintPure, Category = "Duel")
	UE_API float GetRoundTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Duel")
	bool IsMatchFinished() const { return bMatchFinished; }

	UPROPERTY(BlueprintAssignable, Category = "Duel")
	FOnYanDuelScoreChanged OnScoreChanged;

	UPROPERTY(BlueprintAssignable, Category = "Duel")
	FOnYanDuelMatchFinished OnMatchFinished;

	/**
	 * 新回合开始时广播，仅服务器。
	 *
	 * 场地机关随回合复位这件事只能由赛制这一侧发起：机关不知道上一回合是怎么结束的，
	 * 而回合可能因出界提前结束，按固定时长自行计时必然与实际回合错位。
	 */
	UPROPERTY(BlueprintAssignable, Category = "Duel")
	FOnYanDuelRoundStarted OnRoundStarted;

	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End UActorComponent Interface

protected:
	/** 单回合时长上限（秒），到点按负荷较低者判分 */
	UPROPERTY(EditDefaultsOnly, Category = "Duel", Meta = (ClampMin = "1.0", ForceUnits = "s"))
	float RoundTimeLimit = 90.f;

	/** 赢下一局所需的分数 */
	UPROPERTY(EditDefaultsOnly, Category = "Duel", Meta = (ClampMin = "1"))
	int32 PointsToWinSet = 3;

	/** 赢下整场所需的局数 */
	UPROPERTY(EditDefaultsOnly, Category = "Duel", Meta = (ClampMin = "1"))
	int32 SetsToWinMatch = 2;

	/**
	 * 双方同时出界的判定窗口（秒）。
	 * 追杀方与被追方的出界总是一前一后到达，窗口太短会把同归于尽算成一次干净的击杀。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Duel", Meta = (ClampMin = "0.0", ForceUnits = "s"))
	float DoubleOutWindow = 0.6f;

	/** 回合结算到下一回合开始之间的间隔（秒），须不短于死亡技能的重生延时 */
	UPROPERTY(EditDefaultsOnly, Category = "Duel", Meta = (ClampMin = "0.0", ForceUnits = "s"))
	float InterRoundDelay = 3.5f;

private:
	// 重置计时并把双方拉回起点，开始新回合
	UE_API void StartRound();

	// 双亡窗口到期后统一裁决：窗口内出界的一律不得分，其余各得一分
	UE_API void ResolveOutOfBounds();

	// 回合超时：负荷较低者得分，持平则无人得分
	UE_API void HandleRoundTimeout();

	// 给指定玩家加一分，并推进局与场
	UE_API void AwardPoint(APlayerState* Scorer);

	// 回合结束后的收尾：安排下一回合，或在分出胜负时停在结束状态
	UE_API void FinishRound();

	// 清空双方负荷与能量，并让仍在场上的一方归位
	UE_API void ResetPlayersForNewRound();

	// 按 PlayerState 取（或新建）其成绩条目
	UE_API FYanDuelPlayerScore& FindOrAddScore(APlayerState* PlayerState);

	UFUNCTION()
	UE_API void OnRep_Scores();

	// 解析玩家的 ASC，用于读负荷与重置资源
	static UE_API UAbilitySystemComponent* GetPlayerAbilitySystem(APlayerState* PlayerState);

	UPROPERTY(ReplicatedUsing = OnRep_Scores)
	TArray<FYanDuelPlayerScore> Scores;

	UPROPERTY(Replicated)
	bool bMatchFinished = false;

	/** 本回合结束的世界时刻，供客户端算倒计时 */
	UPROPERTY(Replicated)
	float RoundEndWorldTime = 0.f;

	// 本回合是否仍在进行；结算开始后置假，挡掉后续出界通知
	bool bRoundActive = false;

	// 双亡窗口内已出界的玩家
	TArray<TObjectPtr<APlayerState>> PendingOutPlayers;

	FTimerHandle RoundTimeoutTimer;
	FTimerHandle DoubleOutTimer;
	FTimerHandle NextRoundTimer;
};

#undef UE_API
