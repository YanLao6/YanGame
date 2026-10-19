#include "GameMode/YanDuelRoundComponent.h"

#include "AbilitySystemComponent.h"
#include "AttributeSets/YanImpulseSet.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameMode/ModularExperienceGameMode.h"
#include "Messages/VerbMessageHelpers.h"
#include "ModularAbilityTags.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanDuelRoundComponent)

UYanDuelRoundComponent::UYanDuelRoundComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
}

void UYanDuelRoundComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UYanDuelRoundComponent, Scores);
	DOREPLIFETIME(UYanDuelRoundComponent, bMatchFinished);
	DOREPLIFETIME(UYanDuelRoundComponent, RoundEndWorldTime);
}

void UYanDuelRoundComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		StartRound();
	}
}

void UYanDuelRoundComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RoundTimeoutTimer);
		World->GetTimerManager().ClearTimer(DoubleOutTimer);
		World->GetTimerManager().ClearTimer(NextRoundTimer);
	}

	Super::EndPlay(EndPlayReason);
}

float UYanDuelRoundComponent::GetRoundTimeRemaining() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr || RoundEndWorldTime <= 0.f)
	{
		return 0.f;
	}

	return FMath::Max(0.f, RoundEndWorldTime - World->GetTimeSeconds());
}

void UYanDuelRoundComponent::StartRound()
{
	UWorld* World = GetWorld();
	if (World == nullptr || bMatchFinished)
	{
		return;
	}

	PendingOutPlayers.Reset();
	ResetPlayersForNewRound();

	bRoundActive      = true;
	RoundEndWorldTime = World->GetTimeSeconds() + RoundTimeLimit;

	World->GetTimerManager().SetTimer(RoundTimeoutTimer, this, &UYanDuelRoundComponent::HandleRoundTimeout, RoundTimeLimit, /*bLoop=*/false);

	// 放在最后广播：机关复位时回合计时已就位，它们据此算自己的时刻才不会偏
	OnRoundStarted.Broadcast();
}

void UYanDuelRoundComponent::NotifyPlayerOut(APlayerState* OutPlayer)
{
	if (OutPlayer == nullptr || !bRoundActive)
	{
		return;
	}

	PendingOutPlayers.AddUnique(OutPlayer);

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// 首个出界者开窗，此后窗口内的出界一并计入本次裁决；窗口不因后续出界而延长，
	// 否则交替出界可以把一个回合无限拖下去
	if (!World->GetTimerManager().IsTimerActive(DoubleOutTimer))
	{
		// 超时判分与出界判分不能同时成立，出界一经发生即停掉超时
		World->GetTimerManager().ClearTimer(RoundTimeoutTimer);
		World->GetTimerManager().SetTimer(DoubleOutTimer, this, &UYanDuelRoundComponent::ResolveOutOfBounds, FMath::Max(DoubleOutWindow, KINDA_SMALL_NUMBER), /*bLoop=*/false);
	}
}

void UYanDuelRoundComponent::ResolveOutOfBounds()
{
	if (!bRoundActive)
	{
		return;
	}

	bRoundActive = false;

	const AGameStateBase* GameState = GetGameState<AGameStateBase>();
	if (GameState == nullptr)
	{
		FinishRound();
		return;
	}

	// 窗口内没出界的才得分。双方都出界时无人满足此条件，自然就是不计分
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (PlayerState != nullptr && !PendingOutPlayers.Contains(PlayerState))
		{
			AwardPoint(PlayerState);
		}
	}

	FinishRound();
}

void UYanDuelRoundComponent::HandleRoundTimeout()
{
	if (!bRoundActive)
	{
		return;
	}

	bRoundActive = false;

	const AGameStateBase* GameState = GetGameState<AGameStateBase>();
	if (GameState == nullptr)
	{
		FinishRound();
		return;
	}

	// 负荷较低者得分：龟缩一整个回合不挨打确实能赢，但对手同样不挨打时就没有便宜可占
	APlayerState* LowestLoadPlayer = nullptr;
	float         LowestLoad       = TNumericLimits<float>::Max();
	bool          bTied            = false;

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		const UAbilitySystemComponent* ASC = GetPlayerAbilitySystem(PlayerState);
		if (ASC == nullptr)
		{
			continue;
		}

		const float Load = ASC->GetNumericAttribute(UYanImpulseSet::GetLoadAttribute());
		if (Load < LowestLoad)
		{
			LowestLoad       = Load;
			LowestLoadPlayer = PlayerState;
			bTied            = false;
		}
		else if (FMath::IsNearlyEqual(Load, LowestLoad))
		{
			bTied = true;
		}
	}

	if (!bTied && LowestLoadPlayer != nullptr)
	{
		AwardPoint(LowestLoadPlayer);
	}

	FinishRound();
}

void UYanDuelRoundComponent::AwardPoint(APlayerState* Scorer)
{
	FYanDuelPlayerScore& Score = FindOrAddScore(Scorer);
	++Score.RoundPoints;

	if (Score.RoundPoints >= PointsToWinSet)
	{
		++Score.SetsWon;

		// 一局结束，双方的局内分数一同清零
		for (FYanDuelPlayerScore& Entry : Scores)
		{
			Entry.RoundPoints = 0;
		}

		if (Score.SetsWon >= SetsToWinMatch)
		{
			bMatchFinished = true;
			OnMatchFinished.Broadcast(Scorer);
		}
	}

	OnScoreChanged.Broadcast();
}

void UYanDuelRoundComponent::FinishRound()
{
	RoundEndWorldTime = 0.f;

	if (bMatchFinished)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(NextRoundTimer, this, &UYanDuelRoundComponent::StartRound, FMath::Max(InterRoundDelay, KINDA_SMALL_NUMBER), /*bLoop=*/false);
	}
}

void UYanDuelRoundComponent::ResetPlayersForNewRound()
{
	const AGameStateBase* GameState = GetGameState<AGameStateBase>();
	UWorld*               World     = GetWorld();
	if (GameState == nullptr || World == nullptr)
	{
		return;
	}

	AModularExperienceGameMode* GameMode = World->GetAuthGameMode<AModularExperienceGameMode>();

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		UAbilitySystemComponent* ASC = GetPlayerAbilitySystem(PlayerState);
		if (ASC == nullptr)
		{
			continue;
		}

		// 首个回合在 GameState 的 BeginPlay 上开始，早于 Experience 把冲量属性集授予各玩家。
		// 此时无须重置：属性集授予时本就带着满能量与零负荷的初值，正是这里要写入的状态
		if (ASC->HasAttributeSetForAttribute(UYanImpulseSet::GetLoadAttribute()))
		{
			ASC->ApplyModToAttribute(UYanImpulseSet::GetLoadAttribute(), EGameplayModOp::Override, 0.f);
			ASC->ApplyModToAttribute(UYanImpulseSet::GetChargeAttribute(), EGameplayModOp::Override, ASC->GetNumericAttribute(UYanImpulseSet::GetMaxChargeAttribute()));
		}

		// 出界方的重生由死亡技能自己安排，此处再请求一次会重置它的定时器
		if (ASC->HasMatchingGameplayTag(ModularAbilityTags::Status_Death_Dead))
		{
			continue;
		}

		if (GameMode != nullptr)
		{
			if (AController* Controller = PlayerState->GetOwningController())
			{
				// 先销毁旧 Pawn 再请求重生：两者都在服务器上时，新 Pawn 的
				// UModularAbilityExtensionComponent 会撞上「ASC 已有一个仍具权威的 Avatar」，
				// 那条 ensure 假定这种重叠只可能出现在客户端。回合复位本就该让旧身体消失
				if (APawn* OldPawn = Controller->GetPawn())
				{
					OldPawn->Destroy();
				}

				GameMode->RequestPlayerRestartNextFrame(Controller, /*bForceReset=*/true);
			}
		}
	}
}

FYanDuelPlayerScore& UYanDuelRoundComponent::FindOrAddScore(APlayerState* PlayerState)
{
	for (FYanDuelPlayerScore& Entry : Scores)
	{
		if (Entry.PlayerState == PlayerState)
		{
			return Entry;
		}
	}

	FYanDuelPlayerScore NewEntry;
	NewEntry.PlayerState = PlayerState;

	return Scores[Scores.Add(NewEntry)];
}

void UYanDuelRoundComponent::OnRep_Scores()
{
	OnScoreChanged.Broadcast();
}

UAbilitySystemComponent* UYanDuelRoundComponent::GetPlayerAbilitySystem(APlayerState* PlayerState)
{
	return UVerbMessageHelpers::GetAbilitySystemComponentFromObject(PlayerState);
}
