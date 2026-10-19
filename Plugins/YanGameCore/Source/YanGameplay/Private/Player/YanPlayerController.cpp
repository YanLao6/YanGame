// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/YanPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpectatorPawn.h"
#include "Engine/GameInstance.h"
#include "GameMode/ModularExperienceGameState.h"
#include "ModularTeams.h"
#include "Net/UnrealNetwork.h"
#include "Player/YanPlayerState.h"
#include "YanReplaySubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanPlayerController)

bool AYanPlayerController::ShouldRecordClientReplay()
{
	const UWorld* World = GetWorld();
	return World
		&& !World->IsPlayingReplay()
		&& !IsRunningDedicatedServer()
		&& IsLocalPlayerController()
		&& UYanReplaySubsystem::DoesPlatformSupportReplays()
		&& UYanReplaySubsystem::IsAutoRecordEnabledFor(this);
}

bool AYanPlayerController::TryToRecordClientReplay()
{
	if (!ShouldRecordClientReplay())
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UYanReplaySubsystem* ReplaySubsystem = GameInstance ? GameInstance->GetSubsystem<UYanReplaySubsystem>() : nullptr;
	if (!ReplaySubsystem)
	{
		return false;
	}

	// 只由首个本地玩家录制，避免分屏时重复录制同一场对局
	if (GameInstance->GetFirstLocalPlayerController() != this)
	{
		return false;
	}

	if (AModularExperienceGameState* GameState = Cast<AModularExperienceGameState>(GetWorld()->GetGameState()))
	{
		// 登记为回放的观察目标，本地回放据此确定跟随视角
		GameState->SetRecorderPlayerState(PlayerState);
		ReplaySubsystem->RecordClientReplay(this);
		return true;
	}

	return false;
}

AYanPlayerState* AYanPlayerController::GetYanPlayerState() const
{
	return Cast<AYanPlayerState>(PlayerState);
}

void AYanPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 观战目标由客户端随时切换，服务端无法预知，故弃用只对 Owner 复制的 TargetViewRotation，
	// 改由被观战者的 PlayerState 广播视角旋转（见 AYanPlayerState::ReplicatedViewRotation）。
	DISABLE_REPLICATED_PROPERTY(APlayerController, TargetViewRotation);
}

void AYanPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (PlayerCameraManager == nullptr)
	{
		return;
	}

	APawn* TargetPawn = PlayerCameraManager->GetViewTargetPawn();
	if (TargetPawn == nullptr)
	{
		return;
	}

	// 自己的视角旋转上报给自己的 PlayerState，供他人观战时读取
	if (HasAuthority() || TargetPawn->IsLocallyControlled())
	{
		if (AYanPlayerState* YanPlayerState = GetYanPlayerState())
		{
			YanPlayerState->SetReplicatedViewRotation(TargetPawn->GetViewRotation());
		}
	}

	// 观战他人时，从被观战者的 PlayerState 取回视角旋转
	if (!TargetPawn->IsLocallyControlled())
	{
		if (const AYanPlayerState* TargetPlayerState = TargetPawn->GetPlayerState<AYanPlayerState>())
		{
			TargetViewRotation = TargetPlayerState->GetReplicatedViewRotation();
		}
	}

	FollowSpectateTargetWithSpectatorPawn(TargetPawn);
}

void AYanPlayerController::FollowSpectateTargetWithSpectatorPawn(const APawn* TargetPawn)
{
	ASpectatorPawn* Spectator = GetSpectatorPawn();
	if (Spectator == nullptr || Spectator == TargetPawn)
	{
		return;
	}

	// 服务端在观战状态下用客户端上报的 SpectatorPawn 位置做网络相关性判定
	// （见 APlayerController::GetPlayerViewPoint），不跟随会把被观战者判为不相关而剔除。
	Spectator->SetActorLocation(TargetPawn->GetActorLocation(), /*bSweep=*/false, /*OutSweepHitResult=*/nullptr, ETeleportType::TeleportPhysics);
}

bool AYanPlayerController::IsSpectating() const
{
	return IsInState(NAME_Spectating);
}

void AYanPlayerController::EnterSpectate()
{
	if (HasAuthority())
	{
		AuthEnterSpectate();
	}
	else
	{
		ServerEnterSpectate();
	}
}

void AYanPlayerController::LeaveSpectate()
{
	if (HasAuthority())
	{
		AuthLeaveSpectate();
	}
	else
	{
		ServerLeaveSpectate();
	}
}

void AYanPlayerController::ServerEnterSpectate_Implementation()
{
	AuthEnterSpectate();
}

void AYanPlayerController::ServerLeaveSpectate_Implementation()
{
	AuthLeaveSpectate();
}

void AYanPlayerController::AuthEnterSpectate()
{
	if (IsSpectating())
	{
		return;
	}

	SpectateReturnPawn = GetPawn();

	// 与 StartSpectatingOnly 相反：本状态可逆，须放行后续的重生请求
	bPlayerIsWaiting = true;

	// 进入观战无对称的引擎入口，服务端与客户端各自切换一次状态
	ChangeState(NAME_Spectating);
	ClientGotoState(NAME_Spectating);
}

void AYanPlayerController::AuthLeaveSpectate()
{
	if (!IsSpectating())
	{
		return;
	}

	APawn* ReturnPawn = SpectateReturnPawn.Get();
	SpectateReturnPawn.Reset();

	// Possess 内部会切回 Playing 并驱动客户端 ClientRestart，无需手动切状态
	if (IsValid(ReturnPawn) && ReturnPawn->GetController() == nullptr)
	{
		Possess(ReturnPawn);
		return;
	}

	// 原 Pawn 已失效（如死亡销毁），退回常规重生流程
	AGameModeBase* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode() : nullptr;
	if (IsValid(GameMode) && GameMode->PlayerCanRestart(this))
	{
		GameMode->RestartPlayer(this);
	}
}

void AYanPlayerController::SpectateNextPlayer()
{
	if (IsSpectating())
	{
		ServerViewNextPlayer();
	}
}

void AYanPlayerController::SpectatePreviousPlayer()
{
	if (IsSpectating())
	{
		ServerViewPrevPlayer();
	}
}

void AYanPlayerController::SpectateFreeCamera()
{
	if (IsSpectating())
	{
		// ViewTarget 回到 PlayerController 后，相机取 SpectatorPawn 位置与 ControlRotation
		ServerViewSelf();
	}
}

void AYanPlayerController::SpectatePlayerState(APlayerState* TargetPlayerState)
{
	if (IsSpectating())
	{
		ServerSpectatePlayerState(TargetPlayerState);
	}
}

void AYanPlayerController::ServerSpectatePlayerState_Implementation(APlayerState* TargetPlayerState)
{
	if (!IsSpectating() || TargetPlayerState == nullptr)
	{
		return;
	}

	AGameModeBase* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode() : nullptr;
	if (GameMode == nullptr || !GameMode->CanSpectate(this, TargetPlayerState))
	{
		return;
	}

	// 目标须有 Pawn，否则镜头会落到无处可看的 PlayerState 上
	const AController* TargetController = Cast<AController>(TargetPlayerState->GetOwner());
	if (TargetController == nullptr || TargetController->GetPawn() == nullptr)
	{
		return;
	}

	// 传 PlayerState 而非 Pawn：被观战者重生换 Pawn 后镜头会自动跟上（见 FTViewTarget::CheckViewTarget）
	SetViewTarget(TargetPlayerState);
}

APlayerState* AYanPlayerController::GetSpectateTargetPlayerState() const
{
	return PlayerCameraManager != nullptr ? PlayerCameraManager->ViewTarget.GetPlayerState() : nullptr;
}

void AYanPlayerController::SetSpectatorStartLocation(const FVector& Location, const FRotator& Rotation)
{
	SetInitialLocationAndRotation(Location, Rotation);

	if (!IsLocalController())
	{
		ClientSetSpectatorStartLocation(Location, Rotation);
	}
}

void AYanPlayerController::ClientSetSpectatorStartLocation_Implementation(FVector Location, FRotator Rotation)
{
	// 已生成的 SpectatorPawn 会被瞬移过去，尚未生成的则按新的 SpawnLocation 生成
	SetInitialLocationAndRotation(Location, Rotation);
}

void AYanPlayerController::InitPlayerState()
{
	Super::InitPlayerState();
	BroadcastOnPlayerStateChanged();
}

void AYanPlayerController::CleanupPlayerState()
{
	Super::CleanupPlayerState();
	BroadcastOnPlayerStateChanged();
}

void AYanPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BroadcastOnPlayerStateChanged();
}

void AYanPlayerController::OnPlayerStateChanged()
{
}

void AYanPlayerController::BroadcastOnPlayerStateChanged()
{
	OnPlayerStateChanged();

	// 从旧 PlayerState 上摘除监听
	FGenericTeamId OldTeamID = FGenericTeamId::NoTeam;
	if (LastSeenPlayerState != nullptr)
	{
		if (IModularTeamAgentInterface* PlayerStateTeamInterface = Cast<IModularTeamAgentInterface>(LastSeenPlayerState))
		{
			OldTeamID = PlayerStateTeamInterface->GetGenericTeamId();
			PlayerStateTeamInterface->GetTeamChangedDelegateChecked().RemoveAll(this);
		}
	}

	// 挂到新 PlayerState 上
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

void AYanPlayerController::OnPlayerStateChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam)
{
	ConditionalBroadcastTeamChanged(this, IntegerToGenericTeamId(OldTeam), IntegerToGenericTeamId(NewTeam));
}

void AYanPlayerController::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	UE_LOG(LogModularTeams, Error, TEXT("You can't set the team ID on a player controller (%s); it's driven by the associated player state"), *GetPathNameSafe(this));
}

FGenericTeamId AYanPlayerController::GetGenericTeamId() const
{
	if (const IModularTeamAgentInterface* PSWithTeamInterface = Cast<IModularTeamAgentInterface>(PlayerState))
	{
		return PSWithTeamInterface->GetGenericTeamId();
	}

	return FGenericTeamId::NoTeam;
}

FOnModularTeamIndexChangedDelegate* AYanPlayerController::GetOnTeamIndexChangedDelegate()
{
	return &OnTeamChangedDelegate;
}
