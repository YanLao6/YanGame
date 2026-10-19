// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/CancellableAsyncAction.h"
#include "UObject/ScriptInterface.h"
#include "UObject/WeakInterfacePtr.h"

#include "AsyncAction_ObserveTeam.generated.h"

#define UE_API MODULARTEAMS_API

class IModularTeamAgentInterface;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularTeamObservedAsyncDelegate, bool, bTeamSet, int32, TeamId);

/** 监听指定对象的队伍变化。 */
UCLASS(MinimalAPI)
class UAsyncAction_ObserveTeam : public UCancellableAsyncAction
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * 监听队伍代理的队伍变化。
	 *
	 * 会立即广播一次当前归属，随后持续监听后续变化。
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", Keywords = "Watch"))
	static UE_API UAsyncAction_ObserveTeam* ObserveTeam(UObject* TeamAgent);

	//~Begin UBlueprintAsyncActionBase Interface
	UE_API virtual void Activate() override;
	UE_API virtual void SetReadyToDestroy() override;
	//~End UBlueprintAsyncActionBase Interface

public:
	// 队伍被设置或变更时触发
	UPROPERTY(BlueprintAssignable)
	FModularTeamObservedAsyncDelegate OnTeamChanged;

private:
	static UAsyncAction_ObserveTeam* InternalObserveTeamChanges(TScriptInterface<IModularTeamAgentInterface> TeamActor);

	UFUNCTION()
	void OnWatchedAgentChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam);

	TWeakInterfacePtr<IModularTeamAgentInterface> TeamInterfacePtr;
};

#undef UE_API
