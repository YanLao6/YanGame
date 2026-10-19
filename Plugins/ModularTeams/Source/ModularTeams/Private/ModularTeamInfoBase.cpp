// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularTeamInfoBase.h"

#include "Engine/World.h"
#include "ModularTeamSubsystem.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularTeamInfoBase)

class FLifetimeProperty;

AModularTeamInfoBase::AModularTeamInfoBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TeamId(INDEX_NONE)
{
	bReplicates = true;
	bAlwaysRelevant = true;
	NetPriority = 3.0f;
	SetReplicatingMovement(false);
}

void AModularTeamInfoBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, TeamTags);
	DOREPLIFETIME_CONDITION(ThisClass, TeamId, COND_InitialOnly);
}

void AModularTeamInfoBase::BeginPlay()
{
	Super::BeginPlay();

	TryRegisterWithTeamSubsystem();
}

void AModularTeamInfoBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (TeamId != INDEX_NONE)
	{
		// EndPlay 可能发生在子系统已销毁之后，此处不能假定子系统存在
		if (UModularTeamSubsystem* TeamSubsystem = GetWorld()->GetSubsystem<UModularTeamSubsystem>())
		{
			TeamSubsystem->UnregisterTeamInfo(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AModularTeamInfoBase::RegisterWithTeamSubsystem(UModularTeamSubsystem* Subsystem)
{
	Subsystem->RegisterTeamInfo(this);
}

void AModularTeamInfoBase::TryRegisterWithTeamSubsystem()
{
	if (TeamId != INDEX_NONE)
	{
		UModularTeamSubsystem* TeamSubsystem = GetWorld()->GetSubsystem<UModularTeamSubsystem>();
		if (ensure(TeamSubsystem))
		{
			RegisterWithTeamSubsystem(TeamSubsystem);
		}
	}
}

void AModularTeamInfoBase::SetTeamId(int32 NewTeamId)
{
	check(HasAuthority());
	check(TeamId == INDEX_NONE);
	check(NewTeamId != INDEX_NONE);

	TeamId = NewTeamId;

	TryRegisterWithTeamSubsystem();
}

void AModularTeamInfoBase::OnRep_TeamId()
{
	TryRegisterWithTeamSubsystem();
}
