#include "HeroSelectionBoardComponent.h"

#include "HeroAssignmentSystem.h"
#include "CommonSessionSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameMode/ModularUserFacingExperienceDefinition.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HeroSelectionBoardComponent)

UHeroSelectionBoardComponent::UHeroSelectionBoardComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UHeroSelectionBoardComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UHeroSelectionBoardComponent, Entries);
}

bool UHeroSelectionBoardComponent::SetSelection(APlayerState* Player, const FPrimaryAssetId& HeroId)
{
	const AActor* Owner = GetOwner();
	if (!Player || !Owner || !Owner->HasAuthority())
	{
		return false;
	}

	if (IsHeroTakenByOther(HeroId, Player))
	{
		return false;
	}

	FHeroSelectionEntry& Entry = FindOrAddEntry(Player);

	// 确认后改选会让队友看到的结果与开局结果不一致，改选须先取消确认
	if (Entry.bReady)
	{
		return false;
	}

	if (Entry.HeroId == HeroId)
	{
		return true;
	}

	Entry.HeroId = HeroId;

	// 复制不会回调到写入端，服务器上的本地玩家需在此手动广播
	OnRep_Entries();

	return true;
}

void UHeroSelectionBoardComponent::SetReady(APlayerState* Player, bool bInReady)
{
	const AActor* Owner = GetOwner();
	if (!Player || !Owner || !Owner->HasAuthority())
	{
		return;
	}

	FHeroSelectionEntry& Entry = FindOrAddEntry(Player);
	if (bInReady && !Entry.HeroId.IsValid())
	{
		return;
	}

	if (Entry.bReady == bInReady)
	{
		return;
	}

	Entry.bReady = bInReady;

	OnRep_Entries();

	if (CanStartMatch())
	{
		StartMatchTravel();
	}
}

FPrimaryAssetId UHeroSelectionBoardComponent::GetSelectedHero(const APlayerState* Player) const
{
	const FHeroSelectionEntry* Entry = FindEntry(Player);
	return Entry ? Entry->HeroId : FPrimaryAssetId();
}

bool UHeroSelectionBoardComponent::IsHeroTakenByOther(const FPrimaryAssetId& HeroId, const APlayerState* Requester) const
{
	if (!HeroId.IsValid())
	{
		return false;
	}

	for (const FHeroSelectionEntry& Entry : Entries)
	{
		if (Entry.Player && Entry.Player.Get() != Requester && Entry.HeroId == HeroId)
		{
			return true;
		}
	}

	return false;
}

bool UHeroSelectionBoardComponent::IsPlayerReady(APlayerState* Player) const
{
	const FHeroSelectionEntry* Entry = FindEntry(Player);
	return Entry && Entry->bReady;
}

int32 UHeroSelectionBoardComponent::GetReadyPlayerCount() const
{
	int32 ReadyCount = 0;
	for (const FHeroSelectionEntry& Entry : Entries)
	{
		if (Entry.Player && Entry.bReady)
		{
			++ReadyCount;
		}
	}

	return ReadyCount;
}

int32 UHeroSelectionBoardComponent::GetActivePlayerCount() const
{
	const AGameStateBase* GameState = GetGameState<AGameStateBase>();
	if (!GameState)
	{
		return 0;
	}

	int32 ActiveCount = 0;
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (PlayerState && !PlayerState->IsOnlyASpectator() && !PlayerState->IsInactive())
		{
			++ActiveCount;
		}
	}

	return ActiveCount;
}

void UHeroSelectionBoardComponent::OnRep_Entries()
{
	OnBoardChanged.Broadcast();
}

FHeroSelectionEntry& UHeroSelectionBoardComponent::FindOrAddEntry(APlayerState* Player)
{
	CompactEntries();

	for (FHeroSelectionEntry& Entry : Entries)
	{
		if (Entry.Player == Player)
		{
			return Entry;
		}
	}

	FHeroSelectionEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Player               = Player;

	return NewEntry;
}

const FHeroSelectionEntry* UHeroSelectionBoardComponent::FindEntry(const APlayerState* Player) const
{
	if (!Player)
	{
		return nullptr;
	}

	return Entries.FindByPredicate([Player](const FHeroSelectionEntry& Entry) { return Entry.Player.Get() == Player; });
}

bool UHeroSelectionBoardComponent::CanStartMatch() const
{
	const AGameStateBase* GameState = GetGameState<AGameStateBase>();
	if (!GameState)
	{
		return false;
	}

	// 以在场玩家为准而非看板条目，离场玩家留下的条目不应顶替人数
	int32 ActiveCount = 0;
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (!PlayerState || PlayerState->IsOnlyASpectator() || PlayerState->IsInactive())
		{
			continue;
		}

		const FHeroSelectionEntry* Entry = FindEntry(PlayerState);
		if (!Entry || !Entry->bReady || !Entry->HeroId.IsValid())
		{
			return false;
		}

		++ActiveCount;
	}

	return ActiveCount >= RequiredPlayerCount;
}

void UHeroSelectionBoardComponent::StartMatchTravel()
{
	UWorld* World = GetWorld();
	if (bMatchTravelStarted || !World)
	{
		return;
	}

	if (!NextExperience)
	{
		UE_LOG(LogHeroAssignment, Warning, TEXT("选人已全部完成，但未配置 NextExperience，无法进入对局。"));
		return;
	}

	// 复用 Hosting 请求构造 URL：其中的 Experience 参数正是对局关卡据以加载 Experience 的依据
	const UCommonSession_HostSessionRequest* Request = NextExperience->CreateHostingRequest(this);
	const FString                            URL     = Request ? Request->ConstructTravelURL() : FString();
	if (URL.IsEmpty())
	{
		UE_LOG(LogHeroAssignment,
		       Warning,
		       TEXT("Experience [%s] 未能构造有效的切图 URL，请检查其 MapID 是否为已注册的主资产。"),
		       *GetNameSafe(NextExperience));
		return;
	}

	bMatchTravelStarted = true;

	UE_LOG(LogHeroAssignment, Log, TEXT("选人完成，切换到对局关卡 [%s]。"), *URL);

	World->ServerTravel(URL);
}

void UHeroSelectionBoardComponent::CompactEntries()
{
	Entries.RemoveAll([](const FHeroSelectionEntry& Entry) { return Entry.Player == nullptr; });
}
