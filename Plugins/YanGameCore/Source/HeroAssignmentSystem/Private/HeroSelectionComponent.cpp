#include "HeroSelectionComponent.h"

#include "HeroAssignmentSystem.h"
#include "HeroCatalog.h"
#include "HeroSelectionBoardComponent.h"
#include "HeroSelectionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HeroSelectionComponent)

UHeroSelectionComponent::UHeroSelectionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UHeroSelectionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UHeroSelectionComponent, SelectedHeroId);
}

int32 UHeroSelectionComponent::GetSelectedHeroIndex() const
{
	return HeroCatalog ? HeroCatalog->IndexOfHero(SelectedHeroId) : INDEX_NONE;
}

void UHeroSelectionComponent::RequestSelectHero(FPrimaryAssetId HeroId)
{
	ServerSelectHero(HeroId);
}

void UHeroSelectionComponent::RequestSelectHeroByIndex(int32 Index)
{
	if (!HeroCatalog)
	{
		return;
	}

	const FPrimaryAssetId HeroId = HeroCatalog->GetHeroIdAt(Index);
	if (HeroId.IsValid())
	{
		ServerSelectHero(HeroId);
	}
}

void UHeroSelectionComponent::RequestSetReady(bool bInReady)
{
	ServerSetReady(bInReady);
}

bool UHeroSelectionComponent::IsReady() const
{
	UHeroSelectionBoardComponent* Board = GetSelectionBoard();
	return Board && Board->IsPlayerReady(GetOwningPlayerState());
}

bool UHeroSelectionComponent::IsHeroIndexTakenByOther(int32 Index) const
{
	const UHeroSelectionBoardComponent* Board = GetSelectionBoard();
	if (!HeroCatalog || !Board)
	{
		return false;
	}

	return Board->IsHeroTakenByOther(HeroCatalog->GetHeroIdAt(Index), GetOwningPlayerState());
}

UHeroSelectionBoardComponent* UHeroSelectionComponent::GetSelectionBoard() const
{
	const UWorld* World         = GetWorld();
	AGameStateBase* GameState   = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->FindComponentByClass<UHeroSelectionBoardComponent>() : nullptr;
}

APlayerState* UHeroSelectionComponent::GetOwningPlayerState() const
{
	const AController* Controller = GetController<AController>();
	return Controller ? Controller->PlayerState : nullptr;
}

void UHeroSelectionComponent::ServerSelectHero_Implementation(FPrimaryAssetId HeroId)
{
	// 只接受目录内的角色，拒绝客户端上报的非法 Id
	if (!HeroCatalog || !HeroCatalog->FindPawnData(HeroId))
	{
		UE_LOG(LogHeroAssignment, Warning, TEXT("拒绝非法的角色选择 [%s]。"), *HeroId.ToString());
		return;
	}

	APlayerState* PlayerState = GetOwningPlayerState();
	if (!PlayerState)
	{
		return;
	}

	// 互斥与确认状态由看板裁决，大厅未注入看板时退化为无互斥的自由选择
	if (UHeroSelectionBoardComponent* Board = GetSelectionBoard())
	{
		if (!Board->SetSelection(PlayerState, HeroId))
		{
			UE_LOG(LogHeroAssignment,
			       Log,
			       TEXT("玩家 [%s] 选择角色 [%s] 未被接受：已被他人选走或本人已确认。"),
			       *PlayerState->GetPlayerName(),
			       *HeroId.ToString());
			return;
		}
	}

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UHeroSelectionSubsystem* Selection = GameInstance ? GameInstance->GetSubsystem<UHeroSelectionSubsystem>() : nullptr;
	if (!Selection)
	{
		return;
	}

	Selection->SetSelectedHero(PlayerState->GetUniqueId(), HeroId);

	SelectedHeroId = HeroId;

	// 复制不会回调到写入端，Listen Server 的本地玩家需在此手动广播
	OnRep_SelectedHeroId();

	UE_LOG(LogHeroAssignment,
	       Log,
	       TEXT("玩家 [%s] 选择角色 [%s]。"),
	       *PlayerState->GetPlayerName(),
	       *HeroId.ToString());
}

void UHeroSelectionComponent::ServerSetReady_Implementation(bool bInReady)
{
	APlayerState*                 PlayerState = GetOwningPlayerState();
	UHeroSelectionBoardComponent* Board       = GetSelectionBoard();
	if (!PlayerState || !Board)
	{
		return;
	}

	Board->SetReady(PlayerState, bInReady);
}

void UHeroSelectionComponent::OnRep_SelectedHeroId()
{
	OnSelectedHeroChanged.Broadcast(SelectedHeroId);
}
