// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModularTeamInfoBase.h"

#include "ModularTeamPrivateInfo.generated.h"

#define UE_API MODULARTEAMS_API

/** 仅对本队伍成员可见的队伍信息。真正的可见性裁剪需由复制图（Replication Graph）实现。 */
UCLASS(MinimalAPI)
class AModularTeamPrivateInfo : public AModularTeamInfoBase
{
	GENERATED_BODY()

public:
	UE_API explicit AModularTeamPrivateInfo(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

#undef UE_API
