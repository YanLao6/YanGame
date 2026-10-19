// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularTeamPublicInfo.h"

#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularTeamPublicInfo)

class FLifetimeProperty;

AModularTeamPublicInfo::AModularTeamPublicInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AModularTeamPublicInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, TeamDisplayAsset, COND_InitialOnly);
}

void AModularTeamPublicInfo::SetTeamDisplayAsset(TObjectPtr<UModularTeamDisplayAsset> NewDisplayAsset)
{
	check(HasAuthority());
	check(TeamDisplayAsset == nullptr);

	TeamDisplayAsset = NewDisplayAsset;

	TryRegisterWithTeamSubsystem();
}

void AModularTeamPublicInfo::OnRep_TeamDisplayAsset()
{
	TryRegisterWithTeamSubsystem();
}
