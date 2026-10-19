// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/YanSettingsShared.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Internationalization/Culture.h"
#include "Misc/App.h"
#include "SubtitleDisplaySubsystem.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanSettingsShared)

static const FString SHARED_SETTINGS_SLOT_NAME = TEXT("SharedGameSettings");

int32 UYanSettingsShared::GetLatestDataVersion() const
{
	return 1;
}

UYanSettingsShared* UYanSettingsShared::CreateTemporarySettings(const ULocalPlayer* LocalPlayer)
{
	// 该实例不来自磁盘，但仍需具备存盘能力
	UYanSettingsShared* SharedSettings = Cast<UYanSettingsShared>(CreateNewSaveGameForLocalPlayer(UYanSettingsShared::StaticClass(), LocalPlayer, SHARED_SETTINGS_SLOT_NAME));

	SharedSettings->ApplySettings();

	return SharedSettings;
}

UYanSettingsShared* UYanSettingsShared::LoadOrCreateSettings(const ULocalPlayer* LocalPlayer)
{
	// 会阻塞主线程直到加载完成
	UYanSettingsShared* SharedSettings = Cast<UYanSettingsShared>(LoadOrCreateSaveGameForLocalPlayer(UYanSettingsShared::StaticClass(), LocalPlayer, SHARED_SETTINGS_SLOT_NAME));

	SharedSettings->ApplySettings();

	return SharedSettings;
}

bool UYanSettingsShared::AsyncLoadOrCreateSettings(const ULocalPlayer* LocalPlayer, FOnSettingsLoadedEvent Delegate)
{
	FOnLocalPlayerSaveGameLoadedNative Lambda = FOnLocalPlayerSaveGameLoadedNative::CreateLambda([Delegate](ULocalPlayerSaveGame* LoadedSave)
		{
			UYanSettingsShared* LoadedSettings = CastChecked<UYanSettingsShared>(LoadedSave);

			LoadedSettings->ApplySettings();

			Delegate.ExecuteIfBound(LoadedSettings);
		});

	return ULocalPlayerSaveGame::AsyncLoadOrCreateSaveGameForLocalPlayer(UYanSettingsShared::StaticClass(), LocalPlayer, SHARED_SETTINGS_SLOT_NAME, Lambda);
}

void UYanSettingsShared::SaveSettings()
{
	// 异步保存，失败可接受
	AsyncSaveGameToSlotForLocalPlayer();

	// 键位映射由增强输入自行序列化，需与共享设置同期落盘
	if (UEnhancedInputLocalPlayerSubsystem* System = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(OwningPlayer))
	{
		if (UEnhancedInputUserSettings* InputSettings = System->GetUserSettings())
		{
			InputSettings->AsyncSaveSettings();
		}
	}
}

void UYanSettingsShared::ApplySettings()
{
	ApplySubtitleOptions();
	ApplyBackgroundAudioSettings();
	ApplyCultureSettings();

	if (UEnhancedInputLocalPlayerSubsystem* System = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(OwningPlayer))
	{
		if (UEnhancedInputUserSettings* InputSettings = System->GetUserSettings())
		{
			InputSettings->ApplySettings();
		}
	}
}

void UYanSettingsShared::ApplySubtitleOptions()
{
	if (USubtitleDisplaySubsystem* SubtitleSystem = USubtitleDisplaySubsystem::Get(OwningPlayer))
	{
		FSubtitleFormat SubtitleFormat;
		SubtitleFormat.SubtitleTextSize          = SubtitleTextSize;
		SubtitleFormat.SubtitleTextColor         = SubtitleTextColor;
		SubtitleFormat.SubtitleTextBorder        = SubtitleTextBorder;
		SubtitleFormat.SubtitleBackgroundOpacity = SubtitleBackgroundOpacity;

		SubtitleSystem->SetSubtitleDisplayOptions(SubtitleFormat);
	}
}

void UYanSettingsShared::SetAllowAudioInBackgroundSetting(EYanAllowBackgroundAudioSetting NewValue)
{
	if (ChangeValueAndDirty(AllowAudioInBackground, NewValue))
	{
		ApplyBackgroundAudioSettings();
	}
}

void UYanSettingsShared::ApplyBackgroundAudioSettings()
{
	// 失焦音量是进程级设置，分屏时只认主玩家的选择
	if (OwningPlayer && OwningPlayer->IsPrimaryPlayer())
	{
		FApp::SetUnfocusedVolumeMultiplier((AllowAudioInBackground != EYanAllowBackgroundAudioSetting::Off) ? 1.0f : 0.0f);
	}
}

const FString& UYanSettingsShared::GetPendingCulture() const
{
	return PendingCulture;
}

void UYanSettingsShared::SetPendingCulture(const FString& NewCulture)
{
	PendingCulture         = NewCulture;
	bResetToDefaultCulture = false;
	bIsDirty               = true;
}

void UYanSettingsShared::OnCultureChanged()
{
	ClearPendingCulture();
	bResetToDefaultCulture = false;
}

void UYanSettingsShared::ClearPendingCulture()
{
	PendingCulture.Reset();
}

bool UYanSettingsShared::IsUsingDefaultCulture() const
{
	FString Culture;
	GConfig->GetString(TEXT("Internationalization"), TEXT("Culture"), Culture, GGameUserSettingsIni);

	return Culture.IsEmpty();
}

void UYanSettingsShared::ResetToDefaultCulture()
{
	ClearPendingCulture();
	bResetToDefaultCulture = true;
	bIsDirty               = true;
}

void UYanSettingsShared::ApplyCultureSettings()
{
	if (bResetToDefaultCulture)
	{
		const FCulturePtr SystemDefaultCulture = FInternationalization::Get().GetDefaultCulture();
		check(SystemDefaultCulture.IsValid());

		if (FInternationalization::Get().SetCurrentCulture(SystemDefaultCulture->GetName()))
		{
			GConfig->RemoveKey(TEXT("Internationalization"), TEXT("Culture"), GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}

		bResetToDefaultCulture = false;
	}
	else if (!PendingCulture.IsEmpty())
	{
		// SetCurrentCulture 可能广播语言变更并回头清空 PendingCulture，故先取副本
		const FString CultureToApply = PendingCulture;
		if (FInternationalization::Get().SetCurrentCulture(CultureToApply))
		{
			// 写入用户配置而非存档：登录之前、加载画面阶段就需要本地化文本
			GConfig->SetString(TEXT("Internationalization"), TEXT("Culture"), *CultureToApply, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}

		ClearPendingCulture();
	}
}

void UYanSettingsShared::ResetCultureToCurrentSettings()
{
	ClearPendingCulture();
	bResetToDefaultCulture = false;
}
