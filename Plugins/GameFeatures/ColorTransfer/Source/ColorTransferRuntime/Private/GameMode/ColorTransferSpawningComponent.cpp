#include "GameMode/ColorTransferSpawningComponent.h"

#include "Attributes/ColorPigmentSet.h"
#include "Actors/ColorTransferCheckpoint.h"
#include "Effects/ColorPigmentEffects.h"
#include "ColorTransferRuntimeModule.h"
#include "Components/ColorPigmentRulesComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "ModularTeamSubsystem.h"
#include "Player/ModularPlayerStart.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorTransferSpawningComponent)

UColorTransferSpawningComponent::UColorTransferSpawningComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TeamStartTags.Add(1, TEXT("Red"));
	TeamStartTags.Add(2, TEXT("Blue"));

	TeamPigments.Add(1, EColorPigment::Red);
	TeamPigments.Add(2, EColorPigment::Blue);
}

void UColorTransferSpawningComponent::BeginPlay()
{
	Super::BeginPlay();

	// GameMode 只取 GameState 上的第一个出生点组件，同时挂了别的出生规则时本组件不会生效，出生时颜料也不会初始化
	const UModularPlayerSpawningComponent* ActiveSpawning = GetGameStateChecked<AGameStateBase>()->FindComponentByClass<UModularPlayerSpawningComponent>();
	if (ActiveSpawning != this)
	{
		UE_LOG(LogColorTransfer, Error, TEXT("出生点组件 %s 抢在 %s 之前，从 Experience 中移除多余的出生规则"), *GetNameSafe(ActiveSpawning), *GetName());
	}
}

AActor* UColorTransferSpawningComponent::OnChoosePlayerStart(AController* Player, TArray<AModularPlayerStart*>& PlayerStarts)
{
	if (Player && Player->PlayerState)
	{
		if (const TWeakObjectPtr<AActor>* Checkpoint = PlayerCheckpoints.Find(Player->PlayerState))
		{
			if (AColorTransferCheckpoint* ValidCheckpoint = Cast<AColorTransferCheckpoint>(Checkpoint->Get()))
			{
				if (PlayerStarts.Contains(ValidCheckpoint))
				{
					return ValidCheckpoint;
				}
			}
		}
	}

	const FName* StartTag = TeamStartTags.Find(FindTeamId(Player));
	if (!StartTag)
	{
		return nullptr;
	}

	TArray<AModularPlayerStart*> TeamStarts = PlayerStarts.FilterByPredicate([StartTag](const AModularPlayerStart* PlayerStart)
	{
		return PlayerStart && PlayerStart->PlayerStartTag == *StartTag;
	});

	if (TeamStarts.IsEmpty())
	{
		return nullptr;
	}

	// 本队出生点全被占用时宁可重叠，也不让基类随机到对方出生点
	APlayerStart* Unoccupied = GetFirstRandomUnoccupiedPlayerStart(Player, TeamStarts);
	return Unoccupied ? Unoccupied : TeamStarts[0];
}

void UColorTransferSpawningComponent::SetPlayerCheckpoint(AController* Player, AActor* Checkpoint)
{
	AColorTransferCheckpoint* ColorCheckpoint = Cast<AColorTransferCheckpoint>(Checkpoint);
	if (!Player || !Player->HasAuthority() || !Player->PlayerState || !ColorCheckpoint)
	{
		return;
	}

	const FName* TeamStartTag = TeamStartTags.Find(FindTeamId(Player));
	if (TeamStartTag && !ColorCheckpoint->PlayerStartTag.IsNone() && ColorCheckpoint->PlayerStartTag != *TeamStartTag)
	{
		return;
	}

	PlayerCheckpoints.Add(Player->PlayerState, ColorCheckpoint);
}

void UColorTransferSpawningComponent::OnFinishRestartPlayer(AController* Player, const FRotator& StartRotation)
{
	Super::OnFinishRestartPlayer(Player, StartRotation);

	const EColorPigment* OwnPigment = TeamPigments.Find(FindTeamId(Player));
	if (!OwnPigment)
	{
		return;
	}

	if (UColorPigmentSet* PigmentSet = UColorPigmentSet::FindForActor(Player))
	{
		ColorTransfer::AuthApplyReset(*PigmentSet, *OwnPigment);
	}
	else
	{
		UE_LOG(LogColorTransfer, Warning, TEXT("%s 出生时没有 UColorPigmentSet，检查 Experience 是否授予了颜料属性集"), *GetNameSafe(Player));
	}

	// 须在颜料重置之后，才能按满额颜料重新裁决
	if (UColorPigmentRulesComponent* Rules = Player->PlayerState ? Player->PlayerState->FindComponentByClass<UColorPigmentRulesComponent>() : nullptr)
	{
		Rules->AuthResetRules();
	}
}

int32 UColorTransferSpawningComponent::FindTeamId(const AController* Player) const
{
	const UModularTeamSubsystem* TeamSubsystem = GetWorld()->GetSubsystem<UModularTeamSubsystem>();
	return TeamSubsystem ? TeamSubsystem->FindTeamFromObject(Player) : INDEX_NONE;
}
