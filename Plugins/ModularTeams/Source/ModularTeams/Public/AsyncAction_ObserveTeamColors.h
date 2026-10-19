// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/CancellableAsyncAction.h"
#include "UObject/WeakInterfacePtr.h"

#include "AsyncAction_ObserveTeamColors.generated.h"

#define UE_API MODULARTEAMS_API

class IModularTeamAgentInterface;
class UModularTeamDisplayAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FModularTeamColorObservedAsyncDelegate, bool, bTeamSet, int32, TeamId, const UModularTeamDisplayAsset*, DisplayAsset);

/** 监听指定对象的队伍配色变化：队伍本身变更，或所属队伍的展示资产被改动。 */
UCLASS(MinimalAPI)
class UAsyncAction_ObserveTeamColors : public UCancellableAsyncAction
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * 监听队伍代理的配色变化。
	 *
	 * 会立即广播一次当前配色，随后持续监听队伍变更与展示资产变更。
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", Keywords = "Watch"))
	static UE_API UAsyncAction_ObserveTeamColors* ObserveTeamColors(UObject* TeamAgent);

	//~Begin UBlueprintAsyncActionBase Interface
	UE_API virtual void Activate() override;
	UE_API virtual void SetReadyToDestroy() override;
	//~End UBlueprintAsyncActionBase Interface

public:
	// 队伍或其配色变更时触发
	UPROPERTY(BlueprintAssignable)
	FModularTeamColorObservedAsyncDelegate OnTeamChanged;

private:
	void BroadcastChange(int32 NewTeam, const UModularTeamDisplayAsset* DisplayAsset);

	UFUNCTION()
	void OnWatchedAgentChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam);

	UFUNCTION()
	void OnDisplayAssetChanged(const UModularTeamDisplayAsset* DisplayAsset);

	TWeakInterfacePtr<IModularTeamAgentInterface> TeamInterfacePtr;
	TWeakObjectPtr<UObject> TeamInterfaceObj;

	int32 LastBroadcastTeamId = INDEX_NONE;
};

#undef UE_API
