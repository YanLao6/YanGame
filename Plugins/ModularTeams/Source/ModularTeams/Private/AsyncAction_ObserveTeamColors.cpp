// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsyncAction_ObserveTeamColors.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "ModularTeamAgentInterface.h"
#include "ModularTeamStatics.h"
#include "ModularTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AsyncAction_ObserveTeamColors)

UAsyncAction_ObserveTeamColors::UAsyncAction_ObserveTeamColors(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAsyncAction_ObserveTeamColors* UAsyncAction_ObserveTeamColors::ObserveTeamColors(UObject* TeamAgent)
{
	UAsyncAction_ObserveTeamColors* Action = nullptr;

	if (TeamAgent != nullptr)
	{
		Action = NewObject<UAsyncAction_ObserveTeamColors>();
		Action->TeamInterfacePtr = TWeakInterfacePtr<IModularTeamAgentInterface>(TeamAgent);
		Action->TeamInterfaceObj = TeamAgent;
		Action->RegisterWithGameInstance(TeamAgent);
	}

	return Action;
}

void UAsyncAction_ObserveTeamColors::SetReadyToDestroy()
{
	Super::SetReadyToDestroy();

	if (IModularTeamAgentInterface* TeamInterface = TeamInterfacePtr.Get())
	{
		TeamInterface->GetTeamChangedDelegateChecked().RemoveAll(this);
	}
}

void UAsyncAction_ObserveTeamColors::Activate()
{
	bool bCouldSucceed = false;
	int32 CurrentTeamIndex = INDEX_NONE;
	UModularTeamDisplayAsset* CurrentDisplayAsset = nullptr;

	if (IModularTeamAgentInterface* TeamInterface = TeamInterfacePtr.Get())
	{
		if (UWorld* World = GEngine->GetWorldFromContextObject(TeamInterfaceObj.Get(), EGetWorldErrorMode::LogAndReturnNull))
		{
			CurrentTeamIndex = GenericTeamIdToInteger(TeamInterface->GetGenericTeamId());
			CurrentDisplayAsset = UModularTeamStatics::GetTeamDisplayAsset(World, CurrentTeamIndex);

			TeamInterface->GetTeamChangedDelegateChecked().AddDynamic(this, &ThisClass::OnWatchedAgentChangedTeam);

			bCouldSucceed = true;
		}
	}

	// 先广播一次当前状态，调用方无需另行读取初值
	BroadcastChange(CurrentTeamIndex, CurrentDisplayAsset);

	// 绑定失败意味着后续不会再有任何更新
	if (!bCouldSucceed)
	{
		SetReadyToDestroy();
	}
}

void UAsyncAction_ObserveTeamColors::BroadcastChange(int32 NewTeam, const UModularTeamDisplayAsset* DisplayAsset)
{
	UWorld* World = GEngine->GetWorldFromContextObject(TeamInterfaceObj.Get(), EGetWorldErrorMode::LogAndReturnNull);
	UModularTeamSubsystem* TeamSubsystem = UWorld::GetSubsystem<UModularTeamSubsystem>(World);

	const bool bTeamChanged = (LastBroadcastTeamId != NewTeam);

	// 换队时先摘掉旧队伍的展示资产监听
	if ((TeamSubsystem != nullptr) && bTeamChanged && (LastBroadcastTeamId != INDEX_NONE))
	{
		TeamSubsystem->GetTeamDisplayAssetChangedDelegate(LastBroadcastTeamId).RemoveAll(this);
	}

	LastBroadcastTeamId = NewTeam;
	OnTeamChanged.Broadcast(NewTeam != INDEX_NONE, NewTeam, DisplayAsset);

	// 再挂上新队伍的监听
	if ((TeamSubsystem != nullptr) && bTeamChanged && (NewTeam != INDEX_NONE))
	{
		TeamSubsystem->GetTeamDisplayAssetChangedDelegate(NewTeam).AddDynamic(this, &ThisClass::OnDisplayAssetChanged);
	}
}

void UAsyncAction_ObserveTeamColors::OnWatchedAgentChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam)
{
	UModularTeamDisplayAsset* DisplayAsset = UModularTeamStatics::GetTeamDisplayAsset(TeamAgent, NewTeam);
	BroadcastChange(NewTeam, DisplayAsset);
}

void UAsyncAction_ObserveTeamColors::OnDisplayAssetChanged(const UModularTeamDisplayAsset* DisplayAsset)
{
	BroadcastChange(LastBroadcastTeamId, DisplayAsset);
}
