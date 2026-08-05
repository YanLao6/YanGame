// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/YanGameMode.h"

#include "HeroAssignmentComponent.h"
#include "YanGameplayHUD.h"
#include "GameMode/YanGameState.h"
#include "Player/ModularExperiencePlayerState.h"
#include "Player/YanPlayerController.h"
#include "Player/YanPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanGameMode)

AYanGameMode::AYanGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerControllerClass = AYanPlayerController::StaticClass();
	PlayerStateClass      = AYanPlayerState::StaticClass();
	HUDClass              = AYanGameplayHUD::StaticClass();
	GameStateClass        = AYanGameState::StaticClass();
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
