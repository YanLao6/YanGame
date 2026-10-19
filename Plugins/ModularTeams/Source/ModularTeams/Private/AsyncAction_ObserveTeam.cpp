// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsyncAction_ObserveTeam.h"

#include "ModularTeamAgentInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AsyncAction_ObserveTeam)

UAsyncAction_ObserveTeam::UAsyncAction_ObserveTeam(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAsyncAction_ObserveTeam* UAsyncAction_ObserveTeam::ObserveTeam(UObject* TeamAgent)
{
	return InternalObserveTeamChanges(TeamAgent);
}

UAsyncAction_ObserveTeam* UAsyncAction_ObserveTeam::InternalObserveTeamChanges(TScriptInterface<IModularTeamAgentInterface> TeamActor)
{
	UAsyncAction_ObserveTeam* Action = nullptr;

	if (TeamActor != nullptr)
	{
		Action = NewObject<UAsyncAction_ObserveTeam>();
		Action->TeamInterfacePtr = TeamActor;
		Action->RegisterWithGameInstance(TeamActor.GetObject());
	}

	return Action;
}

void UAsyncAction_ObserveTeam::SetReadyToDestroy()
{
	Super::SetReadyToDestroy();

	if (IModularTeamAgentInterface* TeamInterface = TeamInterfacePtr.Get())
	{
		TeamInterface->GetTeamChangedDelegateChecked().RemoveAll(this);
	}
}

void UAsyncAction_ObserveTeam::Activate()
{
	bool bCouldSucceed = false;
	int32 CurrentTeamIndex = INDEX_NONE;

	if (IModularTeamAgentInterface* TeamInterface = TeamInterfacePtr.Get())
	{
		CurrentTeamIndex = GenericTeamIdToInteger(TeamInterface->GetGenericTeamId());

		TeamInterface->GetTeamChangedDelegateChecked().AddDynamic(this, &ThisClass::OnWatchedAgentChangedTeam);

		bCouldSucceed = true;
	}

	// 先广播一次当前状态，调用方无需另行读取初值
	OnTeamChanged.Broadcast(CurrentTeamIndex != INDEX_NONE, CurrentTeamIndex);

	// 绑定失败意味着后续不会再有任何更新
	if (!bCouldSucceed)
	{
		SetReadyToDestroy();
	}
}

void UAsyncAction_ObserveTeam::OnWatchedAgentChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam)
{
	OnTeamChanged.Broadcast(NewTeam != INDEX_NONE, NewTeam);
}
