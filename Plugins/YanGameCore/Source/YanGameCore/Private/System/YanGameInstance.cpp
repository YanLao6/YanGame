// Fill out your copyright notice in the Description page of Project Settings.


#include "YanGameCore/Public/System/YanGameInstance.h"


UYanGameInstance::UYanGameInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

int32 UYanGameInstance::AddLocalPlayer(ULocalPlayer* NewPlayer, FPlatformUserId UserId)
{
	return Super::AddLocalPlayer(NewPlayer, UserId);
}

bool UYanGameInstance::RemoveLocalPlayer(ULocalPlayer* ExistingPlayer)
{
	return Super::RemoveLocalPlayer(ExistingPlayer);
}
