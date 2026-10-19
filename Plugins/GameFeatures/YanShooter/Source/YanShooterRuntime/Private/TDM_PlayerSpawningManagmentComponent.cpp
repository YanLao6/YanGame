// 队伍死斗出生点管理组件实现。

#include "TDM_PlayerSpawningManagmentComponent.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "ModularTeamSubsystem.h"
#include "Player/ModularPlayerStart.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TDM_PlayerSpawningManagmentComponent)

UTDM_PlayerSpawningManagmentComponent::UTDM_PlayerSpawningManagmentComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

AActor* UTDM_PlayerSpawningManagmentComponent::OnChoosePlayerStart(AController* Player, TArray<AModularPlayerStart*>& PlayerStarts)
{
	UModularTeamSubsystem* TeamSubsystem = GetWorld()->GetSubsystem<UModularTeamSubsystem>();
	if (!ensure(TeamSubsystem))
	{
		return nullptr;
	}

	const int32 PlayerTeamId = TeamSubsystem->FindTeamFromObject(Player);

	// 登录早期阶段队伍尚未分配，此时不参与出生点选择
	if (!ensure(PlayerTeamId != INDEX_NONE))
	{
		return nullptr;
	}

	const AGameStateBase* GameState = GetGameStateChecked<AGameStateBase>();

	AModularPlayerStart* BestPlayerStart = nullptr;
	double MaxDistance = 0.0;
	AModularPlayerStart* FallbackPlayerStart = nullptr;
	double FallbackMaxDistance = 0.0;

	// PlayerArray 是 TObjectPtr 数组，迭代器产出 const 引用，需按下标取用
	for (int32 PlayerIndex = 0; PlayerIndex < GameState->PlayerArray.Num(); ++PlayerIndex)
	{
		APlayerState* PS = GameState->PlayerArray[PlayerIndex];
		if (PS == nullptr)
		{
			continue;
		}

		const int32 TeamId = TeamSubsystem->FindTeamFromObject(PS);

		if (PS->IsOnlyASpectator() || !ensure(TeamId != INDEX_NONE))
		{
			continue;
		}

		// 只以敌方角色为参照，找离他们最远的出生点
		if (TeamId != PlayerTeamId)
		{
			for (AModularPlayerStart* PlayerStart : PlayerStarts)
			{
				if (APawn* Pawn = PS->GetPawn())
				{
					const double Distance = PlayerStart->GetDistanceTo(Pawn);

					if (PlayerStart->IsClaimed())
					{
						if (FallbackPlayerStart == nullptr || Distance > FallbackMaxDistance)
						{
							FallbackPlayerStart = PlayerStart;
							FallbackMaxDistance = Distance;
						}
					}
					else if (PlayerStart->GetLocationOccupancy(Player) < EModularPlayerStartLocationOccupancy::Full)
					{
						if (BestPlayerStart == nullptr || Distance > MaxDistance)
						{
							BestPlayerStart = PlayerStart;
							MaxDistance = Distance;
						}
					}
				}
			}
		}
	}

	if (BestPlayerStart)
	{
		return BestPlayerStart;
	}

	return FallbackPlayerStart;
}

void UTDM_PlayerSpawningManagmentComponent::OnFinishRestartPlayer(AController* Player, const FRotator& StartRotation)
{
}
