// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/YanGameMode.h"

#include "HeroAssignmentComponent.h"
#include "YanGameplayHUD.h"
#include "GameMode/YanGameState.h"
#include "Player/ModularExperiencePlayerState.h"
#include "Player/YanPlayerController.h"
#include "Player/YanPlayerState.h"
#include "Player/YanSpectatorPawn.h"
#include "UObject/ConstructorHelpers.h"
#include "YanSessionManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanGameMode)

AYanGameMode::AYanGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerControllerClass = AYanPlayerController::StaticClass();
	PlayerStateClass      = AYanPlayerState::StaticClass();
	HUDClass              = AYanGameplayHUD::StaticClass();
	GameStateClass        = AYanGameState::StaticClass();
	SpectatorClass        = AYanSpectatorPawn::StaticClass();

	// 观战输入依赖 InputAction 资产，原生类上未配置；多数关卡直接使用原生 GameMode，
	// 故默认指向配好资产的蓝图子类，否则观战者既不能移动也不能转动视角
	static ConstructorHelpers::FClassFinder<AYanSpectatorPawn> SpectatorPawnFinder(TEXT("/YanGameCore/Player/BP_YanSpectatorPawn"));
	if (SpectatorPawnFinder.Succeeded())
	{
		SpectatorClass = SpectatorPawnFinder.Class;
	}
}

bool AYanGameMode::CanSpectate_Implementation(APlayerController* Viewer, APlayerState* ViewTarget)
{
	// 观战对象须是参战者：纯观战者与已离场的玩家不该出现在切换序列里
	if (ViewTarget == nullptr || ViewTarget->IsOnlyASpectator() || ViewTarget->IsInactive())
	{
		return false;
	}

	return Super::CanSpectate_Implementation(Viewer, ViewTarget);
}

void AYanGameMode::GenericPlayerInitialization(AController* NewPlayer)
{
	// 须在 Super 之前：Super 广播的玩家初始化事件驱动分队，PostLogin 随后按纯观战标记切入观战且不再生成 Pawn
	APlayerController* const PC = Cast<APlayerController>(NewPlayer);

	int32 LobbyTeamId = INDEX_NONE;
	if (PC && PC->PlayerState && !PC->PlayerState->IsABot()
		&& UYanSessionManager::FindRosterTeamId(PC->PlayerState, LobbyTeamId) && LobbyTeamId == INDEX_NONE)
	{
		PC->StartSpectatingOnly();

		// 基类把选点推迟到 RestartPlayer，纯观战者不会走到那里，不补这一步观战视角会停在世界原点。
		// 须在转观战之后选点：此时按观战者处理，随机取起点且不占用
		AYanPlayerController* const YanPC = Cast<AYanPlayerController>(PC);
		if (const AActor* StartSpot = YanPC ? FindPlayerStart(YanPC) : nullptr)
		{
			YanPC->SetSpectatorStartLocation(StartSpot->GetActorLocation(), FRotator(0.0, StartSpot->GetActorRotation().Yaw, 0.0));
		}
	}

	Super::GenericPlayerInitialization(NewPlayer);
}

const UModularPawnData* AYanGameMode::GetPawnDataForController(const AController* InController) const
{
	// PlayerState 已有 PawnData 时直接沿用，保持基类的首写即最终语义
	if (InController)
	{
		if (const AModularExperiencePlayerState* PlayerState = InController->GetPlayerState<AModularExperiencePlayerState>())
		{
			if (const UModularPawnData* PawnData = PlayerState->GetPawnData<UModularPawnData>())
			{
				return PawnData;
			}
		}
	}

	// 角色分配组件的登记结果优先于 Experience 的默认值
	if (GameState)
	{
		if (const UHeroAssignmentComponent* HeroAssignment = GameState->FindComponentByClass<UHeroAssignmentComponent>())
		{
			if (const UModularPawnData* AssignedHero = HeroAssignment->ResolveHeroForPlayer(InController))
			{
				return AssignedHero;
			}

			// 组件接管本局但该玩家尚未登记：返回空以推迟决议，交由组件在玩家初始化时补写。
			// 此处若回退到 Experience 默认值，会锁死 PlayerState 的 PawnData 首写。
			if (HeroAssignment->IsManagingHeroAssignment())
			{
				return nullptr;
			}
		}
	}

	return Super::GetPawnDataForController(InController);
}
