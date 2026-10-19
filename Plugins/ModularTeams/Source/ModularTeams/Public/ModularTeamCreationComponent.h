// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/GameStateComponent.h"

#include "ModularTeamCreationComponent.generated.h"

#define UE_API MODULARTEAMS_API

class AGameModeBase;
class AModularTeamPrivateInfo;
class AModularTeamPublicInfo;
class APlayerState;
class UModularExperienceDefinition;
class UModularTeamDisplayAsset;

/**
 * 建队组件。
 *
 * 挂载于 GameState，在 Experience 加载完成后由服务端创建队伍信息 Actor，并把玩家分配到人数最少的队伍。
 */
UCLASS(MinimalAPI, Blueprintable)
class UModularTeamCreationComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UE_API explicit UModularTeamCreationComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	//~End UActorComponent Interface

private:
	void OnExperienceLoaded(const UModularExperienceDefinition* Experience);

	// 绑定到 GameMode 的动态委托，因此不能置于 WITH_SERVER_CODE 块内
	UFUNCTION()
	void OnPlayerInitialized(AGameModeBase* GameMode, AController* NewPlayer);

protected:
	// 要创建的队伍：队伍编号到展示资产的映射，展示资产可留空
	UPROPERTY(EditDefaultsOnly, Category = Teams)
	TMap<uint8, TObjectPtr<UModularTeamDisplayAsset>> TeamsToCreate;

	UPROPERTY(EditDefaultsOnly, Category = Teams)
	TSubclassOf<AModularTeamPublicInfo> PublicTeamInfoClass;

	UPROPERTY(EditDefaultsOnly, Category = Teams)
	TSubclassOf<AModularTeamPrivateInfo> PrivateTeamInfoClass;

#if WITH_SERVER_CODE
protected:
	UE_API virtual void ServerCreateTeams();
	UE_API virtual void ServerAssignPlayersToTeams();

	/** 为玩家选择队伍；纯观战者会被剥离队伍归属。 */
	UE_API virtual void ServerChooseTeamForPlayer(APlayerState* PS);

private:
	void ServerCreateTeam(int32 TeamId, UModularTeamDisplayAsset* DisplayAsset);

	/** 返回在场人数最少的队伍编号，没有有效队伍时返回 INDEX_NONE。 */
	int32 GetLeastPopulatedTeamID() const;
#endif
};

#undef UE_API
