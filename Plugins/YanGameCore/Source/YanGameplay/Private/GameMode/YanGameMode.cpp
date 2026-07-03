// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/YanGameMode.h"

#include "YanGameplayHUD.h"
#include "GameMode/YanGameState.h"
#include "Player/YanPlayerController.h"
#include "Player/YanPlayerState.h"

AYanGameMode::AYanGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerControllerClass = AYanPlayerController::StaticClass();
	PlayerStateClass      = AYanPlayerState::StaticClass();
	HUDClass              = AYanGameplayHUD::StaticClass();
	GameStateClass        = AYanGameState::StaticClass();
}
