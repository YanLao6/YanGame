// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/YanLocalPlayer.h"

#include "Settings/YanSettingsLocal.h"
#include "Settings/YanSettingsShared.h"
#include "Settings/YanSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanLocalPlayer)

UYanLocalPlayer::UYanLocalPlayer()
{
}

UYanSettingsLocal* UYanLocalPlayer::GetLocalSettings() const
{
	return UYanSettingsLocal::Get();
}

UYanSettingsShared* UYanLocalPlayer::GetSharedSettings() const
{
	return UYanSettingsSubsystem::GetSharedSettingsFor(this);
}
