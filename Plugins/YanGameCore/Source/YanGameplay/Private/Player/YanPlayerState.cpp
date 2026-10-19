// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/YanPlayerState.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "ModularTeams.h"
#include "Net/UnrealNetwork.h"
#include "Player/YanPlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanPlayerState)

AYanPlayerState::AYanPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MyTeamID = FGenericTeamId::NoTeam;
}

AYanPlayerController* AYanPlayerState::GetYanPlayerController() const
{
	return Cast<AYanPlayerController>(GetOwner());
}

void AYanPlayerState::ClientBroadcastMessage_Implementation(const FModularVerbMessage Message)
{
	// 单机模式下该 RPC 会在本地直接执行，此处限定仅客户端广播以免重复
	if (GetNetMode() == NM_Client)
	{
		UGameplayMessageSubsystem::Get(this).BroadcastMessage(Message.Verb, Message);
	}
}

void AYanPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, MyTeamID, SharedParams);
}

void AYanPlayerState::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	if (HasAuthority())
	{
		const FGenericTeamId OldTeamID = MyTeamID;

		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, MyTeamID, this);
		MyTeamID = NewTeamID;
		ConditionalBroadcastTeamChanged(this, OldTeamID, NewTeamID);
	}
	else
	{
		UE_LOG(LogModularTeams, Error, TEXT("Cannot set team for %s on non-authority"), *GetPathName(this));
	}
}

FGenericTeamId AYanPlayerState::GetGenericTeamId() const
{
	return MyTeamID;
}

FOnModularTeamIndexChangedDelegate* AYanPlayerState::GetOnTeamIndexChangedDelegate()
{
	return &OnTeamChangedDelegate;
}

void AYanPlayerState::OnRep_MyTeamID(FGenericTeamId OldTeamID)
{
	ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
}
