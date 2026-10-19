// Bot 控制器实现。

#include "Player/YanPlayerBotController.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameMode/ModularExperienceGameMode.h"
#include "Perception/AIPerceptionComponent.h"
#include "YanShooterLogs.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanPlayerBotController)

AYanPlayerBotController::AYanPlayerBotController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsPlayerState = true;
	bStopAILogicOnUnposses = false;
}

void AYanPlayerBotController::OnPlayerStateChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam)
{
	ConditionalBroadcastTeamChanged(this, IntegerToGenericTeamId(OldTeam), IntegerToGenericTeamId(NewTeam));
}

void AYanPlayerBotController::OnPlayerStateChanged()
{
}

void AYanPlayerBotController::BroadcastOnPlayerStateChanged()
{
	OnPlayerStateChanged();

	FGenericTeamId OldTeamID = FGenericTeamId::NoTeam;
	if (LastSeenPlayerState != nullptr)
	{
		if (IModularTeamAgentInterface* PlayerStateTeamInterface = Cast<IModularTeamAgentInterface>(LastSeenPlayerState))
		{
			OldTeamID = PlayerStateTeamInterface->GetGenericTeamId();
			PlayerStateTeamInterface->GetTeamChangedDelegateChecked().RemoveAll(this);
		}
	}

	FGenericTeamId NewTeamID = FGenericTeamId::NoTeam;
	if (PlayerState != nullptr)
	{
		if (IModularTeamAgentInterface* PlayerStateTeamInterface = Cast<IModularTeamAgentInterface>(PlayerState))
		{
			NewTeamID = PlayerStateTeamInterface->GetGenericTeamId();
			PlayerStateTeamInterface->GetTeamChangedDelegateChecked().AddDynamic(this, &ThisClass::OnPlayerStateChangedTeam);
		}
	}

	ConditionalBroadcastTeamChanged(this, OldTeamID, NewTeamID);

	LastSeenPlayerState = PlayerState;
}

void AYanPlayerBotController::InitPlayerState()
{
	Super::InitPlayerState();
	BroadcastOnPlayerStateChanged();
}

void AYanPlayerBotController::CleanupPlayerState()
{
	Super::CleanupPlayerState();
	BroadcastOnPlayerStateChanged();
}

void AYanPlayerBotController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BroadcastOnPlayerStateChanged();
}

void AYanPlayerBotController::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	UE_LOG(LogYanShooter, Error, TEXT("Bot 控制器 (%s) 的队伍编号由其 PlayerState 驱动，不可直接设置。"), *GetPathNameSafe(this));
}

FGenericTeamId AYanPlayerBotController::GetGenericTeamId() const
{
	if (IModularTeamAgentInterface* PSWithTeamInterface = Cast<IModularTeamAgentInterface>(PlayerState))
	{
		return PSWithTeamInterface->GetGenericTeamId();
	}
	return FGenericTeamId::NoTeam;
}

FOnModularTeamIndexChangedDelegate* AYanPlayerBotController::GetOnTeamIndexChangedDelegate()
{
	return &OnTeamChangedDelegate;
}

void AYanPlayerBotController::ServerRestartController()
{
	if (GetNetMode() == NM_Client)
	{
		return;
	}

	ensure((GetPawn() == nullptr) && IsInState(NAME_Inactive));

	if (IsInState(NAME_Inactive) || (IsInState(NAME_Spectating)))
	{
		AModularExperienceGameMode* const GameMode = GetWorld()->GetAuthGameMode<AModularExperienceGameMode>();

		if ((GameMode == nullptr) || !GameMode->ControllerCanRestart(this))
		{
			return;
		}

		if (GetPawn() != nullptr)
		{
			UnPossess();
		}

		ResetIgnoreInputFlags();

		GameMode->RestartPlayer(this);
	}
}

ETeamAttitude::Type AYanPlayerBotController::GetTeamAttitudeTowards(const AActor& Other) const
{
	if (const APawn* OtherPawn = Cast<APawn>(&Other))
	{
		if (const IModularTeamAgentInterface* TeamAgent = Cast<IModularTeamAgentInterface>(OtherPawn->GetController()))
		{
			const FGenericTeamId OtherTeamID = TeamAgent->GetGenericTeamId();

			if (OtherTeamID.GetId() != GetGenericTeamId().GetId())
			{
				return ETeamAttitude::Hostile;
			}

			return ETeamAttitude::Friendly;
		}
	}

	return ETeamAttitude::Neutral;
}

void AYanPlayerBotController::UpdateTeamAttitude(UAIPerceptionComponent* AIPerception)
{
	if (AIPerception)
	{
		AIPerception->RequestStimuliListenerUpdate();
	}
}

void AYanPlayerBotController::OnUnPossess()
{
	// 解除附身时把 ASC 的 Avatar 一并清掉，避免其继续指向已离开的 Pawn
	if (APawn* PawnBeingUnpossessed = GetPawn())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState))
		{
			if (ASC->GetAvatarActor() == PawnBeingUnpossessed)
			{
				ASC->SetAvatarActor(nullptr);
			}
		}
	}

	Super::OnUnPossess();
}
