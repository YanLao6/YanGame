// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularTeamCreationComponent.h"

#include "ActorComponent/ModularExperienceComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameMode/ModularExperienceGameMode.h"
#include "ModularTeamAgentInterface.h"
#include "ModularTeamPrivateInfo.h"
#include "ModularTeamPublicInfo.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularTeamCreationComponent)

UModularTeamCreationComponent::UModularTeamCreationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PublicTeamInfoClass = AModularTeamPublicInfo::StaticClass();
	PrivateTeamInfoClass = AModularTeamPrivateInfo::StaticClass();
}

void UModularTeamCreationComponent::BeginPlay()
{
	Super::BeginPlay();

	// 队伍必须在其他 Experience Action 之前就位，因此使用高优先级回调
	AGameStateBase* GameState = GetGameStateChecked<AGameStateBase>();
	UModularExperienceComponent* ExperienceComponent = GameState->FindComponentByClass<UModularExperienceComponent>();
	check(ExperienceComponent);
	ExperienceComponent->CallOrRegister_OnExperienceLoaded_HighPriority(FOnModularExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
}

void UModularTeamCreationComponent::OnExperienceLoaded(const UModularExperienceDefinition* Experience)
{
#if WITH_SERVER_CODE
	if (HasAuthority())
	{
		ServerCreateTeams();
		ServerAssignPlayersToTeams();
	}
#endif
}

void UModularTeamCreationComponent::OnPlayerInitialized(AGameModeBase* GameMode, AController* NewPlayer)
{
#if WITH_SERVER_CODE
	check(NewPlayer);
	check(NewPlayer->PlayerState);
	ServerChooseTeamForPlayer(NewPlayer->PlayerState);
#endif
}

#if WITH_SERVER_CODE

void UModularTeamCreationComponent::ServerCreateTeams()
{
	for (const auto& KVP : TeamsToCreate)
	{
		const int32 TeamId = KVP.Key;
		ServerCreateTeam(TeamId, KVP.Value);
	}
}

void UModularTeamCreationComponent::ServerAssignPlayersToTeams()
{
	// 先处理 Experience 加载完成前就已存在的玩家
	AGameStateBase* GameState = GetGameStateChecked<AGameStateBase>();
	for (APlayerState* PS : GameState->PlayerArray)
	{
		ServerChooseTeamForPlayer(PS);
	}

	// 再监听后续登入的玩家
	AModularExperienceGameMode* GameMode = Cast<AModularExperienceGameMode>(GameState->AuthorityGameMode);
	check(GameMode);

	GameMode->OnGameModePlayerInitialized.AddDynamic(this, &ThisClass::OnPlayerInitialized);
}

void UModularTeamCreationComponent::ServerChooseTeamForPlayer(APlayerState* PS)
{
	IModularTeamAgentInterface* PSWithTeamInterface = Cast<IModularTeamAgentInterface>(PS);
	if (PSWithTeamInterface == nullptr)
	{
		return;
	}

	if (PS->IsOnlyASpectator())
	{
		PSWithTeamInterface->SetGenericTeamId(FGenericTeamId::NoTeam);
	}
	else
	{
		const FGenericTeamId TeamID = IntegerToGenericTeamId(GetLeastPopulatedTeamID());
		PSWithTeamInterface->SetGenericTeamId(TeamID);
	}
}

void UModularTeamCreationComponent::ServerCreateTeam(int32 TeamId, UModularTeamDisplayAsset* DisplayAsset)
{
	check(HasAuthority());

	UWorld* World = GetWorld();
	check(World);

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AModularTeamPublicInfo* NewTeamPublicInfo = World->SpawnActor<AModularTeamPublicInfo>(PublicTeamInfoClass, SpawnInfo);
	checkf(NewTeamPublicInfo != nullptr, TEXT("Failed to create public team actor from class %s"), *GetPathNameSafe(*PublicTeamInfoClass));
	NewTeamPublicInfo->SetTeamId(TeamId);
	NewTeamPublicInfo->SetTeamDisplayAsset(DisplayAsset);

	AModularTeamPrivateInfo* NewTeamPrivateInfo = World->SpawnActor<AModularTeamPrivateInfo>(PrivateTeamInfoClass, SpawnInfo);
	checkf(NewTeamPrivateInfo != nullptr, TEXT("Failed to create private team actor from class %s"), *GetPathNameSafe(*PrivateTeamInfoClass));
	NewTeamPrivateInfo->SetTeamId(TeamId);
}

int32 UModularTeamCreationComponent::GetLeastPopulatedTeamID() const
{
	const int32 NumTeams = TeamsToCreate.Num();
	if (NumTeams > 0)
	{
		TMap<int32, uint32> TeamMemberCounts;
		TeamMemberCounts.Reserve(NumTeams);

		for (const auto& KVP : TeamsToCreate)
		{
			const int32 TeamId = KVP.Key;
			TeamMemberCounts.Add(TeamId, 0);
		}

		AGameStateBase* GameState = GetGameStateChecked<AGameStateBase>();
		for (APlayerState* PS : GameState->PlayerArray)
		{
			if (const IModularTeamAgentInterface* PSWithTeamInterface = Cast<IModularTeamAgentInterface>(PS))
			{
				const int32 PlayerTeamID = GenericTeamIdToInteger(PSWithTeamInterface->GetGenericTeamId());

				// 未分配与已断开的玩家不计入人数
				if ((PlayerTeamID != INDEX_NONE) && !PS->IsInactive())
				{
					check(TeamMemberCounts.Contains(PlayerTeamID));
					TeamMemberCounts[PlayerTeamID] += 1;
				}
			}
		}

		// 先比人数，人数相同时取编号较小的队伍
		int32 BestTeamId = INDEX_NONE;
		uint32 BestPlayerCount = TNumericLimits<uint32>::Max();
		for (const auto& KVP : TeamMemberCounts)
		{
			const int32 TestTeamId = KVP.Key;
			const uint32 TestTeamPlayerCount = KVP.Value;

			if (TestTeamPlayerCount < BestPlayerCount)
			{
				BestTeamId = TestTeamId;
				BestPlayerCount = TestTeamPlayerCount;
			}
			else if (TestTeamPlayerCount == BestPlayerCount)
			{
				if ((TestTeamId < BestTeamId) || (BestTeamId == INDEX_NONE))
				{
					BestTeamId = TestTeamId;
					BestPlayerCount = TestTeamPlayerCount;
				}
			}
		}

		return BestTeamId;
	}

	return INDEX_NONE;
}
#endif	// WITH_SERVER_CODE
