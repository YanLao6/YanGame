// Copyright YanLao.

#include "GameMode/YanTeamCreationComponent.h"

#include "GameFramework/PlayerState.h"
#include "ModularTeamAgentInterface.h"
#include "YanSessionManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanTeamCreationComponent)

#if WITH_SERVER_CODE
void UYanTeamCreationComponent::ServerChooseTeamForPlayer(APlayerState* PS)
{
	IModularTeamAgentInterface* const TeamAgent = Cast<IModularTeamAgentInterface>(PS);
	if (TeamAgent && !PS->IsABot() && !PS->IsOnlyASpectator())
	{
		int32 LobbyTeamId = INDEX_NONE;
		if (UYanSessionManager::FindRosterTeamId(PS, LobbyTeamId)
			&& LobbyTeamId >= 0 && LobbyTeamId <= MAX_uint8 && TeamsToCreate.Contains(static_cast<uint8>(LobbyTeamId)))
		{
			TeamAgent->SetGenericTeamId(IntegerToGenericTeamId(LobbyTeamId));
			return;
		}
	}

	Super::ServerChooseTeamForPlayer(PS);
}
#endif
