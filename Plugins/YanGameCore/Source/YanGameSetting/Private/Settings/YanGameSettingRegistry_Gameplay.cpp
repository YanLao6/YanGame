// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditCondition/WhenPlatformHasTrait.h"
#include "EditCondition/WhenPlayingAsPrimaryPlayer.h"
#include "GameSettingCollection.h"
#include "GameSettingValueDiscreteDynamic.h"
#include "NativeGameplayTags.h"
#include "Settings/CustomSettings/YanSettingValueDiscrete_Language.h"
#include "Settings/YanGameSettingRegistry.h"
#include "Settings/YanSettingsLocal.h"

#define LOCTEXT_NAMESPACE "Yan"

// 回放平台支持标签；与 UYanReplaySubsystem 内定义保持一致。此处内联以避免设置模块反向依赖会话模块。
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Platform_Trait_ReplaySupport, "Platform.Trait.ReplaySupport");

UGameSettingCollection* UYanGameSettingRegistry::InitializeGameplaySettings(ULocalPlayer* InLocalPlayer)
{
	UGameSettingCollection* Screen = NewObject<UGameSettingCollection>();
	Screen->SetDevName(TEXT("GameplayCollection"));
	Screen->SetDisplayName(LOCTEXT("GameplayCollection_Name", "游戏"));
	Screen->Initialize(InLocalPlayer);

	// 语言
	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* LanguageSubsection = NewObject<UGameSettingCollection>();
		LanguageSubsection->SetDevName(TEXT("LanguageCollection"));
		LanguageSubsection->SetDisplayName(LOCTEXT("LanguageCollection_Name", "语言"));
		Screen->AddSetting(LanguageSubsection);

		{
			UYanSettingValueDiscrete_Language* Setting = NewObject<UYanSettingValueDiscrete_Language>();
			Setting->SetDevName(TEXT("Language"));
			Setting->SetDisplayName(LOCTEXT("LanguageSetting_Name", "语言"));
			Setting->SetDescriptionRichText(LOCTEXT("LanguageSetting_Description", "游戏使用的语言。"));

#if WITH_EDITOR
			if (GIsEditor)
			{
				// PIE 不会应用语言切换，须以 -game 启动或在编辑器偏好中修改 PIE 语言选项验证
				Setting->SetDescriptionRichText(LOCTEXT("LanguageSetting_WithEditor_Description", "游戏使用的语言。\n\n<text color=\"#ffff00\">注意：语言切换在 PIE 中不会生效，需以 -game 启动测试，或在编辑器偏好设置中修改 PIE 语言选项。</>"));
			}
#endif

			Setting->AddEditCondition(FWhenPlayingAsPrimaryPlayer::Get());

			LanguageSubsection->AddSetting(Setting);
		}
	}

	// 回放
	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* ReplaySubsection = NewObject<UGameSettingCollection>();
		ReplaySubsection->SetDevName(TEXT("ReplayCollection"));
		ReplaySubsection->SetDisplayName(LOCTEXT("ReplayCollection_Name", "回放"));
		Screen->AddSetting(ReplaySubsection);

		{
			UGameSettingValueDiscreteDynamic_Bool* Setting = NewObject<UGameSettingValueDiscreteDynamic_Bool>();
			Setting->SetDevName(TEXT("RecordReplay"));
			Setting->SetDisplayName(LOCTEXT("RecordReplaySetting_Name", "录制回放"));
			Setting->SetDescriptionRichText(LOCTEXT("RecordReplaySetting_Description", "自动录制对局回放。实验性功能，录制的 Demo 回放时可能存在问题。"));

			Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(ShouldAutoRecordReplays));
			Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetShouldAutoRecordReplays));
			Setting->SetDefaultValue(GetDefault<UYanSettingsLocal>()->ShouldAutoRecordReplays());

			Setting->AddEditCondition(FWhenPlayingAsPrimaryPlayer::Get());
			Setting->AddEditCondition(FWhenPlatformHasTrait::KillIfMissing(TAG_Platform_Trait_ReplaySupport, TEXT("Platform does not support saving replays")));

			ReplaySubsection->AddSetting(Setting);
		}

		{
			UGameSettingValueDiscreteDynamic_Number* Setting = NewObject<UGameSettingValueDiscreteDynamic_Number>();
			Setting->SetDevName(TEXT("KeepReplayLimit"));
			Setting->SetDisplayName(LOCTEXT("KeepReplayLimitSetting_Name", "回放保留数量"));
			Setting->SetDescriptionRichText(LOCTEXT("KeepReplayLimitSetting_Description", "本地保留的回放数量，设为 0 表示不限。"));

			Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetNumberOfReplaysToKeep));
			Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetNumberOfReplaysToKeep));
			Setting->SetDefaultValue(GetDefault<UYanSettingsLocal>()->GetNumberOfReplaysToKeep());
			for (int32 Index = 0; Index <= 20; Index++)
			{
				Setting->AddOption(Index, FText::AsNumber(Index));
			}

			Setting->AddEditCondition(FWhenPlayingAsPrimaryPlayer::Get());
			Setting->AddEditCondition(FWhenPlatformHasTrait::KillIfMissing(TAG_Platform_Trait_ReplaySupport, TEXT("Platform does not support saving replays")));

			ReplaySubsection->AddSetting(Setting);
		}
	}

	return Screen;
}

#undef LOCTEXT_NAMESPACE
