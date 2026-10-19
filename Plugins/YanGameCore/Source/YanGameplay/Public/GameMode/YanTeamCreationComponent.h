// Copyright YanLao.

#pragma once

#include "ModularTeamCreationComponent.h"

#include "YanTeamCreationComponent.generated.h"

#define UE_API YANGAMEPLAY_API

/**
 * 建队组件：本局来自等待大厅时，按大厅中的选队分队。
 *
 * 大厅名单由 UYanSessionManager 跨切图保存。名单中的队伍在本组件的 TeamsToCreate 里不存在时，
 * 以及非大厅开局、Bot 等不在名单中的情形，退回按人数最少分队；大厅中未分配的玩家已由 GameMode 标为纯观战者。
 */
UCLASS(MinimalAPI, Blueprintable)
class UYanTeamCreationComponent : public UModularTeamCreationComponent
{
	GENERATED_BODY()

#if WITH_SERVER_CODE
protected:
	//~Begin UModularTeamCreationComponent Interface
	UE_API virtual void ServerChooseTeamForPlayer(APlayerState* PS) override;
	//~End UModularTeamCreationComponent Interface
#endif
};

#undef UE_API
