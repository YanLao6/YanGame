// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/YanSettingsSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "Settings/YanSettingsShared.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanSettingsSubsystem)

void UYanSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 存档槽由 PlatformUserId 决定，旧的 ControllerID 路径下它可能尚未就绪，
	// 此时跳过预热，改由 GetSharedSettings() 兜底加载
	if (GetLocalPlayer()->GetPlatformUserId().IsValid())
	{
		LoadSharedSettingsFromDisk();
	}
}

UYanSettingsShared* UYanSettingsSubsystem::GetSharedSettingsFor(const ULocalPlayer* LocalPlayer)
{
	if (!LocalPlayer)
	{
		return nullptr;
	}

	const UYanSettingsSubsystem* Subsystem = LocalPlayer->GetSubsystem<UYanSettingsSubsystem>();

	return Subsystem ? Subsystem->GetSharedSettings() : nullptr;
}

UYanSettingsShared* UYanSettingsSubsystem::GetSharedSettings() const
{
	if (!SharedSettings)
	{
		const ULocalPlayer* LocalPlayer = GetLocalPlayer<ULocalPlayer>();

		// 桌面平台的存档只读本地磁盘，登录前同步加载是安全的
		if (PLATFORM_DESKTOP)
		{
			SharedSettings = UYanSettingsShared::LoadOrCreateSettings(LocalPlayer);
		}
		else
		{
			// 其余平台须等玩家登录后才能拿到真实存档，先给临时对象
			SharedSettings = UYanSettingsShared::CreateTemporarySettings(LocalPlayer);
		}
	}

	return SharedSettings;
}

void UYanSettingsSubsystem::LoadSharedSettingsFromDisk(bool bForceLoad)
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer<ULocalPlayer>();

	FUniqueNetIdRepl CurrentNetId = LocalPlayer->GetCachedUniqueNetId();
	if (!bForceLoad && SharedSettings && CurrentNetId == NetIdForSharedSettings)
	{
		return;
	}

	ensure(UYanSettingsShared::AsyncLoadOrCreateSettings(LocalPlayer, UYanSettingsShared::FOnSettingsLoadedEvent::CreateUObject(this, &UYanSettingsSubsystem::OnSharedSettingsLoaded)));
}

void UYanSettingsSubsystem::OnSharedSettingsLoaded(UYanSettingsShared* LoadedOrCreatedSettings)
{
	// 设置在进入此处前已经应用完毕
	if (ensure(LoadedOrCreatedSettings))
	{
		// 替换临时对象或上一次加载的对象，旧对象走正常 GC
		SharedSettings = LoadedOrCreatedSettings;

		NetIdForSharedSettings = GetLocalPlayer<ULocalPlayer>()->GetCachedUniqueNetId();
	}
}
