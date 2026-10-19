// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ModularTeamAgentInterface.h"
#include "Player/ModularAbilityPlayerController.h"
#include "YanPlayerController.generated.h"

#define UE_API YANGAMEPLAY_API

class AYanPlayerState;

/**
 * 项目 PlayerController。
 *
 * 队伍归属只读转发自 PlayerState，并在 PlayerState 更替时把队伍变化广播给监听者（如所控 Pawn）。
 */
UCLASS(MinimalAPI)
class AYanPlayerController : public AModularAbilityPlayerController, public IModularTeamAgentInterface
{
	GENERATED_BODY()

public:
	/** 取得 AYanPlayerState（可为 nullptr）。 */
	UE_API AYanPlayerState* GetYanPlayerState() const;

	/**
	 * 进入观战：解除对当前 Pawn 的控制，转入自由观战状态。
	 *
	 * 与引擎 StartSpectatingOnly 的区别在于本状态可逆，玩家仍可经 LeaveSpectate 回到战斗。
	 *
	 * 观战入口标记 Exec 以便从控制台直接驱动，游戏内何时调用由上层决定。
	 */
	UFUNCTION(BlueprintCallable, Exec, Category = "Yan|Spectate")
	UE_API void EnterSpectate();

	/** 退出观战：原 Pawn 仍有效时恢复控制，否则请求重生。 */
	UFUNCTION(BlueprintCallable, Exec, Category = "Yan|Spectate")
	UE_API void LeaveSpectate();

	/** 当前是否处于观战状态。 */
	UFUNCTION(BlueprintPure, Category = "Yan|Spectate")
	UE_API bool IsSpectating() const;

	/** 切到下一个可观战玩家，无可观战对象时维持原样。 */
	UFUNCTION(BlueprintCallable, Exec, Category = "Yan|Spectate")
	UE_API void SpectateNextPlayer();

	/** 切到上一个可观战玩家。 */
	UFUNCTION(BlueprintCallable, Exec, Category = "Yan|Spectate")
	UE_API void SpectatePreviousPlayer();

	/** 指名观战某位玩家，由服务端按 GameMode 的 CanSpectate 校验。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Spectate")
	UE_API void SpectatePlayerState(APlayerState* TargetPlayerState);

	/** 脱离被观战者，转为自由视角。 */
	UFUNCTION(BlueprintCallable, Exec, Category = "Yan|Spectate")
	UE_API void SpectateFreeCamera();

	/** 当前被观战者的 PlayerState，自由视角下为 nullptr。 */
	UFUNCTION(BlueprintPure, Category = "Yan|Spectate")
	UE_API APlayerState* GetSpectateTargetPlayerState() const;

	/**
	 * 把观战视角放到指定位置，仅在服务端调用。
	 *
	 * 远程客户端的 SpectatorPawn 由客户端自行生成，早于 SpawnLocation 复制到达，
	 * 只在服务端设置会使其停在原点，故同时经 Client RPC 在拥有者客户端上重放一次。
	 */
	UE_API void SetSpectatorStartLocation(const FVector& Location, const FRotator& Rotation);

	//~Begin AActor Interface
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

	//~Begin APlayerController Interface
	UE_API virtual void PlayerTick(float DeltaTime) override;
	//~End APlayerController Interface

	//~Begin AController Interface
	UE_API virtual void InitPlayerState() override;
	UE_API virtual void CleanupPlayerState() override;
	UE_API virtual void OnRep_PlayerState() override;
	//~End AController Interface

	//~Begin IModularTeamAgentInterface Interface
	UE_API virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID) override;
	UE_API virtual FGenericTeamId GetGenericTeamId() const override;
	UE_API virtual FOnModularTeamIndexChangedDelegate* GetOnTeamIndexChangedDelegate() override;
	//~End IModularTeamAgentInterface Interface

	/**
	 * 由对局流程调用，在满足条件时开始自动录制客户端回放。
	 * 仅首个本地玩家会录制，并把自身 PlayerState 登记为回放的观察目标。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|PlayerController")
	UE_API bool TryToRecordClientReplay();

	/** 是否应当录制客户端回放；派生类可改写此判断。 */
	UE_API virtual bool ShouldRecordClientReplay();

protected:
	/** PlayerState 发生更替后的扩展点，派生类无需重复挂接各个来源事件。 */
	UE_API virtual void OnPlayerStateChanged();

private:
	// 观战状态切换的服务端实现，本地与远程入口都收敛到这里
	void AuthEnterSpectate();
	void AuthLeaveSpectate();

	UFUNCTION(Server, Reliable)
	void ServerEnterSpectate();

	UFUNCTION(Server, Reliable)
	void ServerLeaveSpectate();

	UFUNCTION(Server, Reliable)
	void ServerSpectatePlayerState(APlayerState* TargetPlayerState);

	UFUNCTION(Client, Reliable)
	void ClientSetSpectatorStartLocation(FVector Location, FRotator Rotation);

	// 观战他人时把 SpectatorPawn 拖到目标身边，避免服务端按陈旧位置剔除被观战者
	void FollowSpectateTargetWithSpectatorPawn(const APawn* TargetPawn);

	// 进入观战前所控 Pawn，退出观战时优先恢复对它的控制
	TWeakObjectPtr<APawn> SpectateReturnPawn;

	// 重新绑定队伍变更监听，并在队伍确实变化时广播
	void BroadcastOnPlayerStateChanged();

	UFUNCTION()
	void OnPlayerStateChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam);

	UPROPERTY()
	FOnModularTeamIndexChangedDelegate OnTeamChangedDelegate;

	// 上一次已绑定的 PlayerState，用于摘除旧监听
	TObjectPtr<APlayerState> LastSeenPlayerState;
};

#undef UE_API
