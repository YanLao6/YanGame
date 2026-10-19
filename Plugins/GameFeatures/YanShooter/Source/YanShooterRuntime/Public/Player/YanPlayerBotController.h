// Bot 控制器声明。

#pragma once

#include "CoreMinimal.h"
#include "ModularAIController.h"
#include "ModularTeamAgentInterface.h"
#include "YanPlayerBotController.generated.h"

namespace ETeamAttitude { enum Type : int; }
struct FGenericTeamId;

class APlayerState;
class UAIPerceptionComponent;

/**
 * Bot 控制器。
 *
 * 供 Bot 使用的 AI 控制器：持有 PlayerState 以便参与计分与队伍归属，
 * 队伍编号由所属 PlayerState 驱动，控制器本身不可直接设置。
 */
UCLASS(MinimalAPI, Blueprintable)
class AYanPlayerBotController : public AModularAIController, public IModularTeamAgentInterface
{
	GENERATED_BODY()

public:
	AYanPlayerBotController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin IModularTeamAgentInterface Interface
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID) override;
	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual FOnModularTeamIndexChangedDelegate* GetOnTeamIndexChangedDelegate() override;
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	//~End IModularTeamAgentInterface Interface

	//~Begin AController Interface
	virtual void OnUnPossess() override;
	//~End AController Interface

	/** 尝试重启本控制器，用于重生。 */
	void ServerRestartController();

	/** 队伍关系变化后刷新 AI 感知，使敌我判定即时生效。 */
	UFUNCTION(BlueprintCallable, Category = "Yan AI Player Controller")
	void UpdateTeamAttitude(UAIPerceptionComponent* AIPerception);

protected:
	/** PlayerState 被设置或清除时触发，供派生类挂接。 */
	virtual void OnPlayerStateChanged();

	//~Begin AController Interface
	virtual void InitPlayerState() override;
	virtual void CleanupPlayerState() override;
	virtual void OnRep_PlayerState() override;
	//~End AController Interface

private:
	UFUNCTION()
	void OnPlayerStateChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam);

	void BroadcastOnPlayerStateChanged();

	UPROPERTY()
	FOnModularTeamIndexChangedDelegate OnTeamChangedDelegate;

	UPROPERTY()
	TObjectPtr<APlayerState> LastSeenPlayerState;
};
