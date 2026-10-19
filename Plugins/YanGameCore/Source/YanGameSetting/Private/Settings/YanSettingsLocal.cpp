// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/YanSettingsLocal.h"

#include "Audio/YanAudioMixEffectsSubsystem.h"
#include "Audio/YanAudioSettings.h"
#include "AudioModulationStatics.h"
#include "CommonUISettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Features/IModularFeatures.h"
#include "Framework/Application/SlateApplication.h"
#include "ICommonUIModule.h"
#include "Misc/App.h"
#include "NativeGameplayTags.h"
#include "Performance/LatencyMarkerModule.h"
#include "SoundControlBus.h"
#include "SoundControlBusMix.h"
#include "Widgets/Layout/SSafeZone.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanSettingsLocal)

namespace YanPerfStatTags
{
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Platform_Trait_SupportsLatencyStats, "Platform.Trait.SupportsLatencyStats");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Platform_Trait_SupportsLatencyMarkers, "Platform.Trait.SupportsLatencyMarkers");
}

namespace YanAudioTags
{
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Platform_Trait_BinauralSettingControlledByOS, "Platform.Trait.BinauralSettingControlledByOS");
}

static TAutoConsoleVariable<bool> CVarApplyFrameRateSettingsInPIE(TEXT("Yan.Settings.ApplyFrameRateSettingsInPIE"),
	false,
	TEXT("是否在 PIE 中应用帧率上限设置"),
	ECVF_Default);

static TAutoConsoleVariable<bool> CVarApplyFrontEndPerformanceOptionsInPIE(TEXT("Yan.Settings.ApplyFrontEndPerformanceOptionsInPIE"),
	false,
	TEXT("是否在 PIE 中应用前端专用的性能设置"),
	ECVF_Default);

// 合并两个帧率上限，取较小值；小于等于 0 视为不限制
static float CombineFrameRateLimits(float Limit1, float Limit2)
{
	if (Limit1 <= 0.0f)
	{
		return Limit2;
	}

	if (Limit2 <= 0.0f)
	{
		return Limit1;
	}

	return FMath::Min(Limit1, Limit2);
}

UYanSettingsLocal::UYanSettingsLocal()
{
	if (!HasAnyFlags(RF_ClassDefaultObject) && FSlateApplication::IsInitialized())
	{
		OnApplicationActivationStateChangedHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this, &ThisClass::OnAppActivationStateChanged);
	}

	SetToDefaults();
}

UYanSettingsLocal* UYanSettingsLocal::Get()
{
	return GEngine ? CastChecked<UYanSettingsLocal>(GEngine->GetGameUserSettings()) : nullptr;
}

void UYanSettingsLocal::BeginDestroy()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(OnApplicationActivationStateChangedHandle);
	}

	Super::BeginDestroy();
}

void UYanSettingsLocal::SetToDefaults()
{
	Super::SetToDefaults();
	SafeZoneScale = 0.0f;

	FrameRateLimit_InMenu = 144.0f;
	FrameRateLimit_WhenBackgrounded = 30.0f;
	FrameRateLimit_OnBattery = 60.0f;

	bEnableLatencyTrackingStats = DoesPlatformSupportLatencyTrackingStats();

	bUseHeadphoneMode = false;
	bDesiredHeadphoneMode = bUseHeadphoneMode;
}

void UYanSettingsLocal::LoadSettings(bool bForceReload)
{
	Super::LoadSettings(bForceReload);

	// 延迟标记模块的启停不随配置自动生效，需在读入配置后主动同步一次
	ApplyLatencyTrackingStatSetting();

	// 双耳渲染的真实开关是 CVar，读入配置后需推送一次才与存档一致
	bDesiredHeadphoneMode = bUseHeadphoneMode;
	SetHeadphoneModeEnabled(bUseHeadphoneMode);
}

float UYanSettingsLocal::GetEffectiveFrameRateLimit()
{
#if WITH_EDITOR
	if (GIsEditor && !CVarApplyFrameRateSettingsInPIE.GetValueOnGameThread())
	{
		return Super::GetEffectiveFrameRateLimit();
	}
#endif

	float EffectiveFrameRateLimit = Super::GetEffectiveFrameRateLimit();

	if (ShouldUseFrontendPerformanceSettings())
	{
		EffectiveFrameRateLimit = CombineFrameRateLimits(EffectiveFrameRateLimit, FrameRateLimit_InMenu);
	}

	if (FPlatformMisc::IsRunningOnBattery())
	{
		EffectiveFrameRateLimit = CombineFrameRateLimits(EffectiveFrameRateLimit, FrameRateLimit_OnBattery);
	}

	if (FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive())
	{
		EffectiveFrameRateLimit = CombineFrameRateLimits(EffectiveFrameRateLimit, FrameRateLimit_WhenBackgrounded);
	}

	if (GetDynamicResolutionFrameRateTarget() != 0.0f)
	{
		EffectiveFrameRateLimit = CombineFrameRateLimits(EffectiveFrameRateLimit, GetDynamicResolutionFrameRateTarget());
	}

	return EffectiveFrameRateLimit;
}

void UYanSettingsLocal::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();

	if (bUseHeadphoneMode != bDesiredHeadphoneMode)
	{
		SetHeadphoneModeEnabled(bDesiredHeadphoneMode);
	}

	if (FApp::CanEverRender())
	{
		ApplyDisplayGamma();
		ApplySafeZoneScale();
		ApplyAntiAliasingMethod();
		UpdateDesktopFramePacing();
	}
}

void UYanSettingsLocal::SetShouldUseFrontendPerformanceSettings(bool bInFrontEnd)
{
	bInFrontEndForPerformancePurposes = bInFrontEnd;
	UpdateEffectiveFrameRateLimit();
}

bool UYanSettingsLocal::ShouldUseFrontendPerformanceSettings() const
{
#if WITH_EDITOR
	if (GIsEditor && !CVarApplyFrontEndPerformanceOptionsInPIE.GetValueOnGameThread())
	{
		return false;
	}
#endif

	return bInFrontEndForPerformancePurposes;
}

bool UYanSettingsLocal::CanRunAutoBenchmark() const
{
	// 桌面平台恒可用；主机/移动端另有各自的画质档位机制
	return true;
}

bool UYanSettingsLocal::ShouldRunAutoBenchmarkAtStartup() const
{
	if (!CanRunAutoBenchmark())
	{
		return false;
	}

	// -1 表示从未跑过基准测试
	return LastCPUBenchmarkResult == -1;
}

void UYanSettingsLocal::RunAutoBenchmark(bool bSaveImmediately)
{
	RunHardwareBenchmark();

	ApplyScalabilitySettings();
	ApplyLatencyTrackingStatSetting();

	if (bSaveImmediately)
	{
		SaveSettings();
	}
}

void UYanSettingsLocal::ApplyScalabilitySettings()
{
	Scalability::SetQualityLevels(ScalabilityQuality);
}

float UYanSettingsLocal::GetSafeZone() const
{
	return FMath::Clamp(SafeZoneScale, 0.0f, 1.0f);
}

void UYanSettingsLocal::SetSafeZone(float Value)
{
	SafeZoneScale = FMath::Clamp(Value, 0.0f, 1.0f);
	ApplySafeZoneScale();
}

void UYanSettingsLocal::ApplySafeZoneScale()
{
	SSafeZone::SetGlobalSafeZoneScale(GetSafeZone());
}

float UYanSettingsLocal::GetDisplayGamma() const
{
	return DisplayGamma;
}

void UYanSettingsLocal::SetDisplayGamma(float InGamma)
{
	DisplayGamma = InGamma;
	ApplyDisplayGamma();
}

void UYanSettingsLocal::ApplyDisplayGamma()
{
	if (GEngine)
	{
		GEngine->DisplayGamma = DisplayGamma;
	}
}

EAntiAliasingMethod UYanSettingsLocal::GetAntiAliasingMethod() const
{
	return AntiAliasingMethod;
}

void UYanSettingsLocal::SetAntiAliasingMethod(EAntiAliasingMethod InMethod)
{
	AntiAliasingMethod = InMethod;
	ApplyAntiAliasingMethod();
}

void UYanSettingsLocal::ApplyAntiAliasingMethod()
{
	// 走 CVar 而非 RendererSettings：后者是项目默认值，改动需重启且对所有玩家生效
	static IConsoleVariable* CVarAntiAliasingMethod = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"));
	if (CVarAntiAliasingMethod)
	{
		CVarAntiAliasingMethod->Set(static_cast<int32>(AntiAliasingMethod.GetValue()), ECVF_SetByGameSetting);
	}
}

float UYanSettingsLocal::GetFrameRateLimit_OnBattery() const
{
	return FrameRateLimit_OnBattery;
}

void UYanSettingsLocal::SetFrameRateLimit_OnBattery(float NewLimitFPS)
{
	FrameRateLimit_OnBattery = NewLimitFPS;
	UpdateEffectiveFrameRateLimit();
}

float UYanSettingsLocal::GetFrameRateLimit_InMenu() const
{
	return FrameRateLimit_InMenu;
}

void UYanSettingsLocal::SetFrameRateLimit_InMenu(float NewLimitFPS)
{
	FrameRateLimit_InMenu = NewLimitFPS;
	UpdateEffectiveFrameRateLimit();
}

float UYanSettingsLocal::GetFrameRateLimit_WhenBackgrounded() const
{
	return FrameRateLimit_WhenBackgrounded;
}

void UYanSettingsLocal::SetFrameRateLimit_WhenBackgrounded(float NewLimitFPS)
{
	FrameRateLimit_WhenBackgrounded = NewLimitFPS;
	UpdateEffectiveFrameRateLimit();
}

float UYanSettingsLocal::GetFrameRateLimit_Always() const
{
	return GetFrameRateLimit();
}

void UYanSettingsLocal::SetFrameRateLimit_Always(float NewLimitFPS)
{
	SetFrameRateLimit(NewLimitFPS);
	UpdateEffectiveFrameRateLimit();
}

float UYanSettingsLocal::GetDynamicResolutionFrameRateTarget() const
{
	return DynamicResolutionFrameTarget;
}

void UYanSettingsLocal::SetDynamicResolutionFrameRateTarget(float NewDynamicResolutionFPS)
{
	if (!IsRunningDedicatedServer())
	{
		DynamicResolutionFrameTarget = NewDynamicResolutionFPS;
	}
}

void UYanSettingsLocal::UpdateEffectiveFrameRateLimit()
{
	if (!IsRunningDedicatedServer())
	{
		SetFrameRateLimitCVar(GetEffectiveFrameRateLimit());
	}
}

void UYanSettingsLocal::UpdateDesktopFramePacing()
{
	// 帧率上限本身由父类依据 UpdateEffectiveFrameRateLimit() 的结果处理，
	// 这里只负责动态分辨率这类派生效果
	const float TargetFPS = GetEffectiveFrameRateLimit();
	const float ClampedFPS = (TargetFPS <= 0.0f) ? 60.0f : TargetFPS;
	UpdateDynamicResFrameTime(ClampedFPS);

	SetDynamicResolutionEnabled(GetDynamicResolutionFrameRateTarget() != 0.0f);

	if (GEngine)
	{
		GEngine->SetDynamicResolutionUserSetting(IsDynamicResolutionEnabled());
	}
}

void UYanSettingsLocal::UpdateDynamicResFrameTime(float TargetFPS)
{
	static IConsoleVariable* CVarDyResFrameTimeBudget = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicRes.FrameTimeBudget"));
	if (CVarDyResFrameTimeBudget)
	{
		if (ensure(TargetFPS > 0.0f))
		{
			const float DyResFrameTimeBudget = 1000.0f / TargetFPS;
			CVarDyResFrameTimeBudget->Set(DyResFrameTimeBudget, ECVF_SetByGameSetting);
		}
	}
}

void UYanSettingsLocal::OnAppActivationStateChanged(bool bIsActive)
{
	// 窗口失焦后应切换到后台帧率上限
	UpdateEffectiveFrameRateLimit();
}

float UYanSettingsLocal::GetOverallVolume() const
{
	return OverallVolume;
}

void UYanSettingsLocal::SetOverallVolume(float InVolume)
{
	OverallVolume = InVolume;

	if (const TObjectPtr<USoundControlBus>* ControlBus = ControlBusMap.Find(TEXT("Overall")))
	{
		SetVolumeForControlBus(*ControlBus, OverallVolume);
	}
}

float UYanSettingsLocal::GetMusicVolume() const
{
	return MusicVolume;
}

void UYanSettingsLocal::SetMusicVolume(float InVolume)
{
	MusicVolume = InVolume;

	if (const TObjectPtr<USoundControlBus>* ControlBus = ControlBusMap.Find(TEXT("Music")))
	{
		SetVolumeForControlBus(*ControlBus, MusicVolume);
	}
}

float UYanSettingsLocal::GetSoundFXVolume() const
{
	return SoundFXVolume;
}

void UYanSettingsLocal::SetSoundFXVolume(float InVolume)
{
	SoundFXVolume = InVolume;

	if (const TObjectPtr<USoundControlBus>* ControlBus = ControlBusMap.Find(TEXT("SoundFX")))
	{
		SetVolumeForControlBus(*ControlBus, SoundFXVolume);
	}
}

float UYanSettingsLocal::GetDialogueVolume() const
{
	return DialogueVolume;
}

void UYanSettingsLocal::SetDialogueVolume(float InVolume)
{
	DialogueVolume = InVolume;

	if (const TObjectPtr<USoundControlBus>* ControlBus = ControlBusMap.Find(TEXT("Dialogue")))
	{
		SetVolumeForControlBus(*ControlBus, DialogueVolume);
	}
}

float UYanSettingsLocal::GetVoiceChatVolume() const
{
	return VoiceChatVolume;
}

void UYanSettingsLocal::SetVoiceChatVolume(float InVolume)
{
	VoiceChatVolume = InVolume;

	if (const TObjectPtr<USoundControlBus>* ControlBus = ControlBusMap.Find(TEXT("VoiceChat")))
	{
		SetVolumeForControlBus(*ControlBus, VoiceChatVolume);
	}
}

bool UYanSettingsLocal::IsHDRAudioModeEnabled() const
{
	return bUseHDRAudioMode;
}

void UYanSettingsLocal::SetHDRAudioModeEnabled(bool bEnabled)
{
	bUseHDRAudioMode = bEnabled;

	if (!GEngine)
	{
		return;
	}

	if (const UWorld* World = GEngine->GetCurrentPlayWorld())
	{
		if (UYanAudioMixEffectsSubsystem* MixEffectsSubsystem = World->GetSubsystem<UYanAudioMixEffectsSubsystem>())
		{
			MixEffectsSubsystem->ApplyDynamicRangeEffectsChains(bEnabled);
		}
	}
}

bool UYanSettingsLocal::IsHeadphoneModeEnabled() const
{
	return bUseHeadphoneMode;
}

void UYanSettingsLocal::SetHeadphoneModeEnabled(bool bEnabled)
{
	if (!CanModifyHeadphoneModeEnabled())
	{
		return;
	}

	static IConsoleVariable* BinauralSpatializationDisabledCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("au.DisableBinauralSpatialization"));
	if (!BinauralSpatializationDisabledCVar)
	{
		return;
	}

	BinauralSpatializationDisabledCVar->Set(!bEnabled, ECVF_SetByGameSetting);

	bUseHeadphoneMode = bEnabled;
}

bool UYanSettingsLocal::CanModifyHeadphoneModeEnabled() const
{
	static IConsoleVariable* BinauralSpatializationDisabledCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("au.DisableBinauralSpatialization"));

	// CVar 已被优先级高于游戏设置的来源写过时，玩家的修改不会生效
	const bool bHRTFOptionAvailable = BinauralSpatializationDisabledCVar
		&& ((BinauralSpatializationDisabledCVar->GetFlags() & EConsoleVariableFlags::ECVF_SetByMask) <= EConsoleVariableFlags::ECVF_SetByGameSetting);

	const bool bBinauralSettingControlledByOS = ICommonUIModule::GetSettings().GetPlatformTraits().HasTag(YanAudioTags::TAG_Platform_Trait_BinauralSettingControlledByOS);

	return bHRTFOptionAvailable && !bBinauralSettingControlledByOS;
}

void UYanSettingsLocal::SetVolumeForControlBus(USoundControlBus* InSoundControlBus, float InVolume)
{
	// 首次通过界面调用 setter 时总线尚未加载
	if (!bSoundControlBusMixLoaded)
	{
		LoadUserControlBusMix();
	}

	if (!GEngine || !InSoundControlBus || !bSoundControlBusMixLoaded)
	{
		return;
	}

	const UWorld* AudioWorld = GEngine->GetCurrentPlayWorld();
	if (!AudioWorld)
	{
		return;
	}

	ensureMsgf(ControlBusMix, TEXT("用户音量混音未能加载"));

	FSoundControlBusMixStage UpdatedControlBusMixStage;
	UpdatedControlBusMixStage.Bus                    = InSoundControlBus;
	UpdatedControlBusMixStage.Value.TargetValue      = InVolume;
	UpdatedControlBusMixStage.Value.AttackTime       = 0.01f;
	UpdatedControlBusMixStage.Value.ReleaseTime      = 0.01f;

	UAudioModulationStatics::UpdateMix(AudioWorld, ControlBusMix, {UpdatedControlBusMixStage});
}

void UYanSettingsLocal::LoadUserControlBusMix()
{
	if (!GEngine)
	{
		return;
	}

	const UWorld* World = GEngine->GetCurrentPlayWorld();
	if (!World)
	{
		return;
	}

	const UYanAudioSettings* AudioSettings = GetDefault<UYanAudioSettings>();
	if (!AudioSettings)
	{
		return;
	}

	ControlBusMap.Empty();

	// 总线名与音量字段一一对应，缺失的条目跳过而非中断整个加载
	auto LoadControlBus = [this](const FSoftObjectPath& Path, const FName Key) -> USoundControlBus*
	{
		if (UObject* ObjPath = Path.TryLoad())
		{
			if (USoundControlBus* Bus = Cast<USoundControlBus>(ObjPath))
			{
				ControlBusMap.Add(Key, Bus);

				return Bus;
			}
		}

		return nullptr;
	};

	USoundControlBus* OverallBus   = LoadControlBus(AudioSettings->OverallVolumeControlBus, TEXT("Overall"));
	USoundControlBus* MusicBus     = LoadControlBus(AudioSettings->MusicVolumeControlBus, TEXT("Music"));
	USoundControlBus* SoundFXBus   = LoadControlBus(AudioSettings->SoundFXVolumeControlBus, TEXT("SoundFX"));
	USoundControlBus* DialogueBus  = LoadControlBus(AudioSettings->DialogueVolumeControlBus, TEXT("Dialogue"));
	USoundControlBus* VoiceChatBus = LoadControlBus(AudioSettings->VoiceChatVolumeControlBus, TEXT("VoiceChat"));

	UObject* MixObjPath = AudioSettings->UserSettingsControlBusMix.TryLoad();
	if (!MixObjPath)
	{
		return;
	}

	USoundControlBusMix* SoundControlBusMix = Cast<USoundControlBusMix>(MixObjPath);
	if (!ensureMsgf(SoundControlBusMix, TEXT("Yan Audio Settings 中缺少用户音量混音引用")))
	{
		return;
	}

	ControlBusMix = SoundControlBusMix;

	const TArray<FSoundControlBusMixStage> ControlBusMixStageArray = {
		UAudioModulationStatics::CreateBusMixStage(World, OverallBus, OverallVolume),
		UAudioModulationStatics::CreateBusMixStage(World, MusicBus, MusicVolume),
		UAudioModulationStatics::CreateBusMixStage(World, SoundFXBus, SoundFXVolume),
		UAudioModulationStatics::CreateBusMixStage(World, DialogueBus, DialogueVolume),
		UAudioModulationStatics::CreateBusMixStage(World, VoiceChatBus, VoiceChatVolume)
	};

	UAudioModulationStatics::UpdateMix(World, ControlBusMix, ControlBusMixStageArray);

	bSoundControlBusMixLoaded = true;
}

EYanStatDisplayMode UYanSettingsLocal::GetPerfStatDisplayState(EYanDisplayablePerformanceStat Stat) const
{
	if (const EYanStatDisplayMode* Mode = DisplayStatList.Find(Stat))
	{
		return *Mode;
	}

	return EYanStatDisplayMode::Hidden;
}

void UYanSettingsLocal::SetPerfStatDisplayState(EYanDisplayablePerformanceStat Stat, EYanStatDisplayMode DisplayMode)
{
	// 隐藏项直接移除，使配置文件只记录真正开启的统计
	if (DisplayMode == EYanStatDisplayMode::Hidden)
	{
		DisplayStatList.Remove(Stat);
	}
	else
	{
		DisplayStatList.FindOrAdd(Stat) = DisplayMode;
	}

	PerfStatSettingsChangedEvent.Broadcast();
}

bool UYanSettingsLocal::DoesPlatformSupportLatencyTrackingStats()
{
	return ICommonUIModule::GetSettings().GetPlatformTraits().HasTag(YanPerfStatTags::TAG_Platform_Trait_SupportsLatencyStats);
}

void UYanSettingsLocal::SetEnableLatencyTrackingStats(const bool bNewVal)
{
	if (bNewVal != bEnableLatencyTrackingStats)
	{
		bEnableLatencyTrackingStats = bNewVal;

		ApplyLatencyTrackingStatSetting();

		LatencyStatIndicatorSettingsChangedEvent.Broadcast();
	}
}

bool UYanSettingsLocal::DoesPlatformSupportLatencyMarkers()
{
	return ICommonUIModule::GetSettings().GetPlatformTraits().HasTag(YanPerfStatTags::TAG_Platform_Trait_SupportsLatencyMarkers);
}

void UYanSettingsLocal::SetEnableLatencyFlashIndicators(const bool bNewVal)
{
	if (bNewVal != bEnableLatencyFlashIndicators)
	{
		bEnableLatencyFlashIndicators = bNewVal;

		LatencyFlashIndicatorSettingsChangedEvent.Broadcast();
	}
}

void UYanSettingsLocal::ApplyLatencyTrackingStatSetting()
{
	// 读取配置时也会走到这里，而无渲染的目标（如 cook 进程）没有可用的延迟标记模块
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	if (!DoesPlatformSupportLatencyTrackingStats())
	{
		return;
	}

	const TArray<ILatencyMarkerModule*> LatencyMarkerModules =
		IModularFeatures::Get().GetModularFeatureImplementations<ILatencyMarkerModule>(ILatencyMarkerModule::GetModularFeatureName());

	for (ILatencyMarkerModule* LatencyMarkerModule : LatencyMarkerModules)
	{
		LatencyMarkerModule->SetEnabled(bEnableLatencyTrackingStats);
	}
}
