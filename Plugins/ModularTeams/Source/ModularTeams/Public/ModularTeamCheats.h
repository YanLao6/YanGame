// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/CheatManager.h"

#include "ModularTeamCheats.generated.h"

#define UE_API MODULARTEAMS_API

/** 队伍相关作弊指令，由队伍子系统在 CheatManager 创建时挂载。 */
UCLASS(MinimalAPI)
class UModularTeamCheats : public UCheatManagerExtension
{
	GENERATED_BODY()

public:
	/** 切到下一支队伍，到末尾后回到第一支。 */
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	UE_API virtual void CycleTeam();

	/** 切到指定队伍。 */
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	UE_API virtual void SetTeam(int32 TeamID);

	/** 打印当前全部队伍编号。 */
	UFUNCTION(Exec)
	UE_API virtual void ListTeams();
};

#undef UE_API
