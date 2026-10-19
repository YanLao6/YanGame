// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditCondition/WhenCondition.h"
#include "EditCondition/WhenPlatformHasTrait.h"
#include "EditCondition/WhenPlayingAsPrimaryPlayer.h"
#include "GameSettingCollection.h"
#include "GameSettingValueDiscreteDynamic.h"
#include "GameSettingValueScalarDynamic.h"
#include "NativeGameplayTags.h"
#include "Settings/CustomSettings/YanSettingValueDiscreteDynamic_AudioOutputDevice.h"
#include "Settings/YanGameSettingRegistry.h"
#include "Settings/YanSettingsLocal.h"
#include "Settings/YanSettingsShared.h"

#define LOCTEXT_NAMESPACE "Yan"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Platform_Trait_SupportsChangingAudioOutputDevice, "Platform.Trait.SupportsChangingAudioOutputDevice");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Platform_Trait_SupportsBackgroundAudio, "Platform.Trait.SupportsBackgroundAudio");

UGameSettingCollection* UYanGameSettingRegistry::InitializeAudioSettings(ULocalPlayer* InLocalPlayer)
{
	UGameSettingCollection* Screen = NewObject<UGameSettingCollection>();
	Screen->SetDevName(TEXT("AudioCollection"));
	Screen->SetDisplayName(LOCTEXT("AudioCollection_Name", "音频"));
	Screen->Initialize(InLocalPlayer);

	// 音量
	// 各路音量写入独立的控制总线，因此总音量与分项音量相乘而非互相覆盖
	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* Volume = NewObject<UGameSettingCollection>();
		Volume->SetDevName(TEXT("VolumeCollection"));
		Volume->SetDisplayName(LOCTEXT("VolumeCollection_Name", "音量"));
		Screen->AddSetting(Volume);

		//----------------------------------------------------------------------------------
		{
			UGameSettingValueScalarDynamic* Setting = NewObject<UGameSettingValueScalarDynamic>();
			Setting->SetDevName(TEXT("OverallVolume"));
			Setting->SetDisplayName(LOCTEXT("OverallVolume_Name", "总音量"));
			Setting->SetDescriptionRichText(LOCTEXT("OverallVolume_Description", "调整游戏内所有声音的音量。"));

			Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetOverallVolume));
			Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetOverallVolume));
			Setting->SetDefaultValue(GetDefault<UYanSettingsLocal>()->GetOverallVolume());
			Setting->SetDisplayFormat(UGameSettingValueScalarDynamic::ZeroToOnePercent);

			Setting->AddEditCondition(FWhenPlayingAsPrimaryPlayer::Get());

			Volume->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueScalarDynamic* Setting = NewObject<UGameSettingValueScalarDynamic>();
			Setting->SetDevName(TEXT("MusicVolume"));
			Setting->SetDisplayName(LOCTEXT("MusicVolume_Name", "音乐"));
			Setting->SetDescriptionRichText(LOCTEXT("MusicVolume_Description", "调整背景音乐的音量。"));

			Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetMusicVolume));
			Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetMusicVolume));
			Setting->SetDefaultValue(GetDefault<UYanSettingsLocal>()->GetMusicVolume());
			Setting->SetDisplayFormat(UGameSettingValueScalarDynamic::ZeroToOnePercent);

			Setting->AddEditCondition(FWhenPlayingAsPrimaryPlayer::Get());

			Volume->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueScalarDynamic* Setting = NewObject<UGameSettingValueScalarDynamic>();
			Setting->SetDevName(TEXT("SoundEffectsVolume"));
			Setting->SetDisplayName(LOCTEXT("SoundEffectsVolume_Name", "音效"));
			Setting->SetDescriptionRichText(LOCTEXT("SoundEffectsVolume_Description", "调整音效的音量。"));

			Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetSoundFXVolume));
			Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetSoundFXVolume));
			Setting->SetDefaultValue(GetDefault<UYanSettingsLocal>()->GetSoundFXVolume());
			Setting->SetDisplayFormat(UGameSettingValueScalarDynamic::ZeroToOnePercent);

			Setting->AddEditCondition(FWhenPlayingAsPrimaryPlayer::Get());

			Volume->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueScalarDynamic* Setting = NewObject<UGameSettingValueScalarDynamic>();
			Setting->SetDevName(TEXT("DialogueVolume"));
			Setting->SetDisplayName(LOCTEXT("DialogueVolume_Name", "对白"));
			Setting->SetDescriptionRichText(LOCTEXT("DialogueVolume_Description", "调整角色对白与旁白的音量。"));

			Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetDialogueVolume));
			Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetDialogueVolume));
			Setting->SetDefaultValue(GetDefault<UYanSettingsLocal>()->GetDialogueVolume());
			Setting->SetDisplayFormat(UGameSettingValueScalarDynamic::ZeroToOnePercent);

			Setting->AddEditCondition(FWhenPlayingAsPrimaryPlayer::Get());

			Volume->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueScalarDynamic* Setting = NewObject<UGameSettingValueScalarDynamic>();
			Setting->SetDevName(TEXT("VoiceChatVolume"));
			Setting->SetDisplayName(LOCTEXT("VoiceChatVolume_Name", "语音聊天"));
			Setting->SetDescriptionRichText(LOCTEXT("VoiceChatVolume_Description", "调整语音聊天的音量。"));

			Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetVoiceChatVolume));
			Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetVoiceChatVolume));
			Setting->SetDefaultValue(GetDefault<UYanSettingsLocal>()->GetVoiceChatVolume());
			Setting->SetDisplayFormat(UGameSettingValueScalarDynamic::ZeroToOnePercent);

			Setting->AddEditCondition(FWhenPlayingAsPrimaryPlayer::Get());

			Volume->AddSetting(Setting);
		}
	}

	// 声音
	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* Sound = NewObject<UGameSettingCollection>();
		Sound->SetDevName(TEXT("SoundCollection"));
		Sound->SetDisplayName(LOCTEXT("SoundCollection_Name", "声音"));
		Screen->AddSetting(Sound);

		//----------------------------------------------------------------------------------
		{
			// 字幕外观项数量较多且互相关联，收进子页面避免挤占音频主页
			UGameSettingCollectionPage* SubtitlePage = NewObject<UGameSettingCollectionPage>();
			SubtitlePage->SetDevName(TEXT("SubtitlePage"));
			SubtitlePage->SetDisplayName(LOCTEXT("SubtitlePage_Name", "字幕"));
			SubtitlePage->SetDescriptionRichText(LOCTEXT("SubtitlePage_Description", "设置字幕的显示样式。"));
			SubtitlePage->SetNavigationText(LOCTEXT("SubtitlePage_Navigation", "选项"));

			SubtitlePage->AddEditCondition(FWhenPlayingAsPrimaryPlayer::Get());

			Sound->AddSetting(SubtitlePage);

			// 字幕
			////////////////////////////////////////////////////////////////////////////////////
			{
				UGameSettingCollection* SubtitleCollection = NewObject<UGameSettingCollection>();
				SubtitleCollection->SetDevName(TEXT("SubtitlesCollection"));
				SubtitleCollection->SetDisplayName(LOCTEXT("SubtitlesCollection_Name", "字幕"));
				SubtitlePage->AddSetting(SubtitleCollection);

				//----------------------------------------------------------------------------------
				{
					UGameSettingValueDiscreteDynamic_Bool* Setting = NewObject<UGameSettingValueDiscreteDynamic_Bool>();
					Setting->SetDevName(TEXT("Subtitles"));
					Setting->SetDisplayName(LOCTEXT("Subtitles_Name", "字幕"));
					Setting->SetDescriptionRichText(LOCTEXT("Subtitles_Description", "开启或关闭字幕。"));

					Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH(GetSubtitlesEnabled));
					Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH(SetSubtitlesEnabled));
					Setting->SetDefaultValue(GetDefault<UYanSettingsShared>()->GetSubtitlesEnabled());

					SubtitleCollection->AddSetting(Setting);
				}
				//----------------------------------------------------------------------------------
				{
					UGameSettingValueDiscreteDynamic_Enum* Setting = NewObject<UGameSettingValueDiscreteDynamic_Enum>();
					Setting->SetDevName(TEXT("SubtitleTextSize"));
					Setting->SetDisplayName(LOCTEXT("SubtitleTextSize_Name", "字号"));
					Setting->SetDescriptionRichText(LOCTEXT("SubtitleTextSize_Description", "选择字幕文字的大小。"));

					Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH(GetSubtitlesTextSize));
					Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH(SetSubtitlesTextSize));
					Setting->SetDefaultValue(GetDefault<UYanSettingsShared>()->GetSubtitlesTextSize());
					Setting->AddEnumOption(ESubtitleDisplayTextSize::ExtraSmall, LOCTEXT("ESubtitleTextSize_ExtraSmall", "极小"));
					Setting->AddEnumOption(ESubtitleDisplayTextSize::Small, LOCTEXT("ESubtitleTextSize_Small", "小"));
					Setting->AddEnumOption(ESubtitleDisplayTextSize::Medium, LOCTEXT("ESubtitleTextSize_Medium", "中"));
					Setting->AddEnumOption(ESubtitleDisplayTextSize::Large, LOCTEXT("ESubtitleTextSize_Large", "大"));
					Setting->AddEnumOption(ESubtitleDisplayTextSize::ExtraLarge, LOCTEXT("ESubtitleTextSize_ExtraLarge", "极大"));

					SubtitleCollection->AddSetting(Setting);
				}
				//----------------------------------------------------------------------------------
				{
					UGameSettingValueDiscreteDynamic_Enum* Setting = NewObject<UGameSettingValueDiscreteDynamic_Enum>();
					Setting->SetDevName(TEXT("SubtitleTextColor"));
					Setting->SetDisplayName(LOCTEXT("SubtitleTextColor_Name", "文字颜色"));
					Setting->SetDescriptionRichText(LOCTEXT("SubtitleTextColor_Description", "选择字幕文字的颜色。"));

					Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH(GetSubtitlesTextColor));
					Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH(SetSubtitlesTextColor));
					Setting->SetDefaultValue(GetDefault<UYanSettingsShared>()->GetSubtitlesTextColor());
					Setting->AddEnumOption(ESubtitleDisplayTextColor::White, LOCTEXT("ESubtitleTextColor_White", "白色"));
					Setting->AddEnumOption(ESubtitleDisplayTextColor::Yellow, LOCTEXT("ESubtitleTextColor_Yellow", "黄色"));

					SubtitleCollection->AddSetting(Setting);
				}
				//----------------------------------------------------------------------------------
				{
					UGameSettingValueDiscreteDynamic_Enum* Setting = NewObject<UGameSettingValueDiscreteDynamic_Enum>();
					Setting->SetDevName(TEXT("SubtitleTextBorder"));
					Setting->SetDisplayName(LOCTEXT("SubtitleTextBorder_Name", "文字描边"));
					Setting->SetDescriptionRichText(LOCTEXT("SubtitleTextBorder_Description", "选择字幕文字的描边方式。"));

					Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH(GetSubtitlesTextBorder));
					Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH(SetSubtitlesTextBorder));
					Setting->SetDefaultValue(GetDefault<UYanSettingsShared>()->GetSubtitlesTextBorder());
					Setting->AddEnumOption(ESubtitleDisplayTextBorder::None, LOCTEXT("ESubtitleTextBorder_None", "无"));
					Setting->AddEnumOption(ESubtitleDisplayTextBorder::Outline, LOCTEXT("ESubtitleTextBorder_Outline", "轮廓"));
					Setting->AddEnumOption(ESubtitleDisplayTextBorder::DropShadow, LOCTEXT("ESubtitleTextBorder_DropShadow", "投影"));

					SubtitleCollection->AddSetting(Setting);
				}
				//----------------------------------------------------------------------------------
				{
					UGameSettingValueDiscreteDynamic_Enum* Setting = NewObject<UGameSettingValueDiscreteDynamic_Enum>();
					Setting->SetDevName(TEXT("SubtitleBackgroundOpacity"));
					Setting->SetDisplayName(LOCTEXT("SubtitleBackgroundOpacity_Name", "背景不透明度"));
					Setting->SetDescriptionRichText(LOCTEXT("SubtitleBackgroundOpacity_Description", "选择字幕背景的不透明度。"));

					Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH(GetSubtitlesBackgroundOpacity));
					Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH(SetSubtitlesBackgroundOpacity));
					Setting->SetDefaultValue(GetDefault<UYanSettingsShared>()->GetSubtitlesBackgroundOpacity());
					Setting->AddEnumOption(ESubtitleDisplayBackgroundOpacity::Clear, LOCTEXT("ESubtitleBackgroundOpacity_Clear", "透明"));
					Setting->AddEnumOption(ESubtitleDisplayBackgroundOpacity::Low, LOCTEXT("ESubtitleBackgroundOpacity_Low", "低"));
					Setting->AddEnumOption(ESubtitleDisplayBackgroundOpacity::Medium, LOCTEXT("ESubtitleBackgroundOpacity_Medium", "中"));
					Setting->AddEnumOption(ESubtitleDisplayBackgroundOpacity::High, LOCTEXT("ESubtitleBackgroundOpacity_High", "高"));
					Setting->AddEnumOption(ESubtitleDisplayBackgroundOpacity::Solid, LOCTEXT("ESubtitleBackgroundOpacity_Solid", "不透明"));

					SubtitleCollection->AddSetting(Setting);
				}
			}
		}
		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscreteDynamic_AudioOutputDevice* Setting = NewObject<UYanSettingValueDiscreteDynamic_AudioOutputDevice>();
			Setting->SetDevName(TEXT("AudioOutputDevice"));
			Setting->SetDisplayName(LOCTEXT("AudioOutputDevice_Name", "音频输出设备"));
			Setting->SetDescriptionRichText(LOCTEXT("AudioOutputDevice_Description", "选择游戏音频的输出设备（不含语音聊天）。"));

			Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetAudioOutputDeviceId));
			Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetAudioOutputDeviceId));

			Setting->AddEditCondition(FWhenPlayingAsPrimaryPlayer::Get());
			Setting->AddEditCondition(FWhenPlatformHasTrait::KillIfMissing(
				TAG_Platform_Trait_SupportsChangingAudioOutputDevice,
				TEXT("当前平台不支持切换音频输出设备"))
			);

			Sound->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueDiscreteDynamic_Enum* Setting = NewObject<UGameSettingValueDiscreteDynamic_Enum>();
			Setting->SetDevName(TEXT("BackgroundAudio"));
			Setting->SetDisplayName(LOCTEXT("BackgroundAudio_Name", "后台音频"));
			Setting->SetDescriptionRichText(LOCTEXT("BackgroundAudio_Description", "游戏切换到后台时是否继续播放声音。开启后，游戏最小化或切换到其他窗口时音频仍会继续。"));

			Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH(GetAllowAudioInBackgroundSetting));
			Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH(SetAllowAudioInBackgroundSetting));
			Setting->SetDefaultValue(GetDefault<UYanSettingsShared>()->GetAllowAudioInBackgroundSetting());

			Setting->AddEnumOption(EYanAllowBackgroundAudioSetting::Off, LOCTEXT("EYanAllowBackgroundAudioSetting_Off", "关闭"));
			Setting->AddEnumOption(EYanAllowBackgroundAudioSetting::AllSounds, LOCTEXT("EYanAllowBackgroundAudioSetting_AllSounds", "全部声音"));

			Setting->AddEditCondition(FWhenPlayingAsPrimaryPlayer::Get());
			Setting->AddEditCondition(FWhenPlatformHasTrait::KillIfMissing(
				TAG_Platform_Trait_SupportsBackgroundAudio,
				TEXT("当前平台不支持后台音频"))
			);

			Sound->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueDiscreteDynamic_Bool* Setting = NewObject<UGameSettingValueDiscreteDynamic_Bool>();
			Setting->SetDevName(TEXT("HeadphoneMode"));
			Setting->SetDisplayName(LOCTEXT("HeadphoneMode_Name", "3D 耳机音效"));
			Setting->SetDescriptionRichText(LOCTEXT("HeadphoneMode_Description", "启用双耳渲染。提供 3D 音频空间化，可更精确地听出声音方位，包括上方、下方与身后。建议仅在使用立体声耳机时开启。"));

			// 读写的是玩家意愿字段，实际的 CVar 推送在 ApplyNonResolutionSettings 中完成
			Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(bDesiredHeadphoneMode));
			Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(bDesiredHeadphoneMode));
			Setting->SetDefaultValue(GetDefault<UYanSettingsLocal>()->IsHeadphoneModeEnabled());

			Setting->AddEditCondition(MakeShared<FWhenCondition>(
				[](const ULocalPlayer*, FGameSettingEditableState& InOutEditState)
				{
					if (!GetDefault<UYanSettingsLocal>()->CanModifyHeadphoneModeEnabled())
					{
						InOutEditState.Kill(TEXT("当前平台不允许修改双耳渲染设置"));
					}
				}));

			Setting->AddEditCondition(FWhenPlayingAsPrimaryPlayer::Get());

			Sound->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueDiscreteDynamic_Bool* Setting = NewObject<UGameSettingValueDiscreteDynamic_Bool>();
			Setting->SetDevName(TEXT("HDRAudioMode"));
			Setting->SetDisplayName(LOCTEXT("HDRAudioMode_Name", "高动态范围音频"));
			Setting->SetDescriptionRichText(LOCTEXT("HDRAudioMode_Description", "启用高动态范围音频。切换运行时的处理链以扩大混音的动态范围，适合影院式或更具电影感的体验。"));

			Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(IsHDRAudioModeEnabled));
			Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetHDRAudioModeEnabled));
			Setting->SetDefaultValue(GetDefault<UYanSettingsLocal>()->IsHDRAudioModeEnabled());

			Setting->AddEditCondition(FWhenPlayingAsPrimaryPlayer::Get());

			Sound->AddSetting(Setting);
		}
	}

	return Screen;
}

#undef LOCTEXT_NAMESPACE
