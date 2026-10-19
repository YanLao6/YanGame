// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Messages/ModularVerbMessage.h"
#include "ModularAbilityPlayerState.h"
#include "ModularTeamAgentInterface.h"
#include "YanPlayerState.generated.h"

#define UE_API YANGAMEPLAY_API

class AYanPlayerController;

/**
 * 项目 PlayerState。
 *
 * 队伍归属的权威载体：Pawn 与 PlayerController 均从这里读取队伍并转发变更通知。
 */
UCLASS(MinimalAPI)
class AYanPlayerState : public AModularAbilityPlayerState, public IModularTeamAgentInterface
{
	GENERATED_BODY()

public:
	UE_API explicit AYanPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 返回所属队伍编号，未分配队伍时为 INDEX_NONE。 */
	UFUNCTION(BlueprintCallable, Category = "Teams")
	int32 GetTeamId() const
	{
		return GenericTeamIdToInteger(MyTeamID);
	}

	/** 返回持有本 PlayerState 的控制器；非本项目控制器类型时为空。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|PlayerState")
	UE_API AYanPlayerController* GetYanPlayerController() const;

	/**
	 * 向该玩家的客户端投递一条消息并在其本地广播。
	 * 走 Unreliable，仅用于嘉奖、提示一类偶尔丢失也无妨的通知。
	 */
	UFUNCTION(Client, Unreliable, BlueprintCallable, Category = "Yan|PlayerState")
	UE_API void ClientBroadcastMessage(const FModularVerbMessage Message);

	//~Begin AActor Interface
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

	//~Begin IModularTeamAgentInterface Interface
	UE_API virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID) override;
	UE_API virtual FGenericTeamId GetGenericTeamId() const override;
	UE_API virtual FOnModularTeamIndexChangedDelegate* GetOnTeamIndexChangedDelegate() override;
	//~End IModularTeamAgentInterface Interface

private:
	UFUNCTION()
	void OnRep_MyTeamID(FGenericTeamId OldTeamID);

	UPROPERTY()
	FOnModularTeamIndexChangedDelegate OnTeamChangedDelegate;

	UPROPERTY(ReplicatedUsing = OnRep_MyTeamID)
	FGenericTeamId MyTeamID;
};

#undef UE_API
