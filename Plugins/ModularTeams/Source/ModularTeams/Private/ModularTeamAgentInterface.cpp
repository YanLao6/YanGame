// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularTeamAgentInterface.h"

#include "GameFramework/Actor.h"
#include "ModularTeams.h"
#include "UObject/ScriptInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularTeamAgentInterface)

UModularTeamAgentInterface::UModularTeamAgentInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void IModularTeamAgentInterface::ConditionalBroadcastTeamChanged(TScriptInterface<IModularTeamAgentInterface> This, FGenericTeamId OldTeamID, FGenericTeamId NewTeamID)
{
	if (OldTeamID != NewTeamID)
	{
		const int32 OldTeamIndex = GenericTeamIdToInteger(OldTeamID);
		const int32 NewTeamIndex = GenericTeamIdToInteger(NewTeamID);

		UObject* ThisObj = This.GetObject();
		const AActor* ContextActor = Cast<AActor>(ThisObj);
		const TCHAR* NetContext = (ContextActor != nullptr) ? (ContextActor->HasAuthority() ? TEXT("Server") : TEXT("Client")) : TEXT("Unknown");
		UE_LOG(LogModularTeams, Verbose, TEXT("[%s] %s assigned team %d"), NetContext, *GetPathNameSafe(ThisObj), NewTeamIndex);

		This.GetInterface()->GetTeamChangedDelegateChecked().Broadcast(ThisObj, OldTeamIndex, NewTeamIndex);
	}
}
