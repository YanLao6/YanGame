// Copyright YanLao.

#include "YanLobbyPlayerComponent.h"

#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "YanLobbyComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanLobbyPlayerComponent)

UYanLobbyPlayerComponent::UYanLobbyPlayerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

UYanLobbyPlayerComponent* UYanLobbyPlayerComponent::FindLobbyPlayer(const APlayerState* PlayerState)
{
	return PlayerState ? PlayerState->FindComponentByClass<UYanLobbyPlayerComponent>() : nullptr;
}

void UYanLobbyPlayerComponent::BeginPlay()
{
	Super::BeginPlay();

	// ListenServer 上房主的控制器是服务器本地控制器，客机的控制器在服务器上不是本地的
	if (HasAuthority())
	{
		const APlayerController* const PC = GetPlayerState<APlayerState>()->GetPlayerController();
		bIsHost = PC && PC->IsLocalController();
	}
}

void UYanLobbyPlayerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, TeamId);
	DOREPLIFETIME(ThisClass, bReady);
	DOREPLIFETIME(ThisClass, bIsHost);
}

void UYanLobbyPlayerComponent::RequestJoinTeam(int32 InTeamId)
{
	ServerRequestJoinTeam(InTeamId);
}

void UYanLobbyPlayerComponent::RequestSetReady(bool bInReady)
{
	ServerRequestSetReady(bInReady);
}

void UYanLobbyPlayerComponent::SendChatMessage(const FString& Message)
{
	ServerSendChatMessage(Message);
}

void UYanLobbyPlayerComponent::AuthSetTeamId(int32 InTeamId)
{
	TeamId = InTeamId;
}

void UYanLobbyPlayerComponent::AuthSetReady(bool bInReady)
{
	bReady = bInReady;
}

void UYanLobbyPlayerComponent::ServerRequestJoinTeam_Implementation(int32 InTeamId)
{
	if (UYanLobbyComponent* Lobby = FindLobby())
	{
		Lobby->AuthJoinTeam(this, InTeamId);
	}
}

void UYanLobbyPlayerComponent::ServerRequestSetReady_Implementation(bool bInReady)
{
	if (UYanLobbyComponent* Lobby = FindLobby())
	{
		Lobby->AuthSetReady(this, bInReady);
	}
}

void UYanLobbyPlayerComponent::ServerSendChatMessage_Implementation(const FString& Message)
{
	if (UYanLobbyComponent* Lobby = FindLobby())
	{
		Lobby->AuthAddChatMessage(GetPlayerState<APlayerState>(), Message);
	}
}

UYanLobbyComponent* UYanLobbyPlayerComponent::FindLobby() const
{
	const UWorld* const World = GetWorld();
	const AGameStateBase* const GameState = World ? World->GetGameState() : nullptr;

	return GameState ? GameState->FindComponentByClass<UYanLobbyComponent>() : nullptr;
}
