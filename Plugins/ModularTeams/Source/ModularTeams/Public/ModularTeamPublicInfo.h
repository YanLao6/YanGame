// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModularTeamInfoBase.h"

#include "ModularTeamPublicInfo.generated.h"

#define UE_API MODULARTEAMS_API

class UModularTeamCreationComponent;
class UModularTeamDisplayAsset;

/** 对所有客户端可见的队伍信息，承载队伍展示资产（配色、图标、短名）。 */
UCLASS(MinimalAPI)
class AModularTeamPublicInfo : public AModularTeamInfoBase
{
	GENERATED_BODY()

	friend UModularTeamCreationComponent;

public:
	UE_API explicit AModularTeamPublicInfo(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UModularTeamDisplayAsset* GetTeamDisplayAsset() const { return TeamDisplayAsset; }

	//~Begin AActor Interface
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

private:
	UFUNCTION()
	void OnRep_TeamDisplayAsset();

	void SetTeamDisplayAsset(TObjectPtr<UModularTeamDisplayAsset> NewDisplayAsset);

private:
	UPROPERTY(ReplicatedUsing = OnRep_TeamDisplayAsset)
	TObjectPtr<UModularTeamDisplayAsset> TeamDisplayAsset;
};

#undef UE_API
