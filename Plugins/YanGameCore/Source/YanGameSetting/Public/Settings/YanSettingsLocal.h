// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameUserSettings.h"
#include "Performance/YanPerformanceStatTypes.h"
#include "SceneUtils.h"

#include "YanSettingsLocal.generated.h"

#define UE_API YANGAMESETTING_API

class USoundControlBus;
class USoundControlBusMix;

/**
 * 机器本地的画面与性能设置，经 GameUserSettings.ini 持久化。
 * 这些值描述的是"这台机器能跑什么"，因此不随玩家账号迁移，与 UYanSettingsShared 相对。
 */
UCLASS(MinimalAPI)
class UYanSettingsLocal : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UE_API UYanSettingsLocal();

	static UE_API UYanSettingsLocal* Get();

	//~Begin UObject Interface
	UE_API virtual void BeginDestroy() override;
	//~End UObject Interface

	//~Begin UGameUserSettings Interface
	UE_API virtual void SetToDefaults() override;
	UE_API virtual void LoadSettings(bool bForceReload) override;
	UE_API virtual float GetEffectiveFrameRateLimit() override;
	UE_API virtual void ApplyNonResolutionSettings() override;
	//~End UGameUserSettings Interface

	//////////////////////////////////////////////////////////////////
	// 前端状态

public:
	/** 主菜单等前端界面使用独立的帧率上限，避免静态画面下满负载运行 */
	UE_API void SetShouldUseFrontendPerformanceSettings(bool bInFrontEnd);

protected:
	bool ShouldUseFrontendPerformanceSettings() const;

private:
	bool bInFrontEndForPerformancePurposes = false;

	//////////////////////////////////////////////////////////////////
	// 画质基准测试

public:
	/** 当前平台是否支持自动画质基准测试 */
	UFUNCTION(BlueprintCallable, Category = Settings)
	UE_API bool CanRunAutoBenchmark() const;

	/** 从未跑过基准测试时返回 true，用于首次启动自动推荐画质 */
	UFUNCTION(BlueprintCallable, Category = Settings)
	UE_API bool ShouldRunAutoBenchmarkAtStartup() const;

	/** 执行硬件基准测试并应用推荐画质档位 */
	UFUNCTION(BlueprintCallable, Category = Settings)
	UE_API void RunAutoBenchmark(bool bSaveImmediately);

	/** 仅应用画质分级，不改动分辨率 */
	UE_API void ApplyScalabilitySettings();

	//////////////////////////////////////////////////////////////////
	// 亮度

public:
	UFUNCTION()
	UE_API float GetDisplayGamma() const;

	UFUNCTION()
	UE_API void SetDisplayGamma(float InGamma);

private:
	void ApplyDisplayGamma();

	UPROPERTY(Config)
	float DisplayGamma = 2.2f;

public:
	/** 返回界面安全区比例，未校准时使用完整显示区域。 */
	UFUNCTION(BlueprintPure, Category = "Yan|Settings")
	UE_API float GetSafeZone() const;

	/** 更新本机界面安全区并立即应用到 Slate。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Settings")
	UE_API void SetSafeZone(float Value);

private:
	void ApplySafeZoneScale();

	UPROPERTY(Config)
	float SafeZoneScale = 0.0f;

	//////////////////////////////////////////////////////////////////
	// 抗锯齿方法
	// 与 Scalability 的 AntiAliasingQuality 正交：此项决定用哪种算法，那项决定该算法的质量档位

public:
	UFUNCTION()
	UE_API EAntiAliasingMethod GetAntiAliasingMethod() const;

	UFUNCTION()
	UE_API void SetAntiAliasingMethod(EAntiAliasingMethod InMethod);

private:
	void ApplyAntiAliasingMethod();

	/** 默认 TSR：UE5 的默认时域上采样算法，画质与开销均衡 */
	UPROPERTY(Config)
	TEnumAsByte<EAntiAliasingMethod> AntiAliasingMethod = AAM_TSR;

	//////////////////////////////////////////////////////////////////
	// 帧率上限
	// 分场景上限最终取最小值合并，0 表示不限制

public:
	UFUNCTION()
	UE_API float GetFrameRateLimit_OnBattery() const;

	UFUNCTION()
	UE_API void SetFrameRateLimit_OnBattery(float NewLimitFPS);

	UFUNCTION()
	UE_API float GetFrameRateLimit_InMenu() const;

	UFUNCTION()
	UE_API void SetFrameRateLimit_InMenu(float NewLimitFPS);

	UFUNCTION()
	UE_API float GetFrameRateLimit_WhenBackgrounded() const;

	UFUNCTION()
	UE_API void SetFrameRateLimit_WhenBackgrounded(float NewLimitFPS);

	UFUNCTION()
	UE_API float GetFrameRateLimit_Always() const;

	UFUNCTION()
	UE_API void SetFrameRateLimit_Always(float NewLimitFPS);

	UFUNCTION()
	UE_API float GetDynamicResolutionFrameRateTarget() const;

	UFUNCTION()
	UE_API void SetDynamicResolutionFrameRateTarget(float NewDynamicResolutionFPS);

protected:
	void UpdateEffectiveFrameRateLimit();

	// 动态分辨率的帧时间预算跟随目标帧率，须在帧率上限变化后重算
	void UpdateDesktopFramePacing();
	void UpdateDynamicResFrameTime(float TargetFPS);

private:
	UPROPERTY(Config)
	float FrameRateLimit_OnBattery;

	UPROPERTY(Config)
	float FrameRateLimit_InMenu;

	UPROPERTY(Config)
	float FrameRateLimit_WhenBackgrounded;

	//////////////////////////////////////////////////////////////////
	// 音量
	// 数值存于本地配置，实际生效靠写入 AudioModulation 的控制总线；
	// 总线资产在 UYanAudioSettings 中登记，首次调用 setter 时惰性加载

public:
	UFUNCTION()
	UE_API float GetOverallVolume() const;

	UFUNCTION()
	UE_API void SetOverallVolume(float InVolume);

	UFUNCTION()
	UE_API float GetMusicVolume() const;

	UFUNCTION()
	UE_API void SetMusicVolume(float InVolume);

	UFUNCTION()
	UE_API float GetSoundFXVolume() const;

	UFUNCTION()
	UE_API void SetSoundFXVolume(float InVolume);

	UFUNCTION()
	UE_API float GetDialogueVolume() const;

	UFUNCTION()
	UE_API void SetDialogueVolume(float InVolume);

	UFUNCTION()
	UE_API float GetVoiceChatVolume() const;

	UFUNCTION()
	UE_API void SetVoiceChatVolume(float InVolume);

	/** 是否启用高动态范围音频，决定采用哪套 Submix 效果链 */
	UFUNCTION()
	UE_API bool IsHDRAudioModeEnabled() const;

	UFUNCTION()
	UE_API void SetHDRAudioModeEnabled(bool bEnabled);

	//////////////////////////////////////////////////////////////////
	// 双耳渲染（HRTF）
	// 实际开关是引擎 CVar au.DisableBinauralSpatialization，
	// 因此玩家的意愿与生效值分开存放：意愿即时记录，直到 ApplyNonResolutionSettings 才推送到 CVar

public:
	/** 双耳渲染当前是否生效 */
	UFUNCTION()
	UE_API bool IsHeadphoneModeEnabled() const;

	/** 写入双耳渲染开关；若 CVar 已被更高优先级来源锁定则不生效 */
	UFUNCTION()
	UE_API void SetHeadphoneModeEnabled(bool bEnabled);

	/** 当前平台是否允许玩家自行调整双耳渲染 */
	UFUNCTION()
	UE_API bool CanModifyHeadphoneModeEnabled() const;

	/** 玩家期望的双耳渲染开关，设置界面直接读写此字段 */
	UPROPERTY(Transient)
	bool bDesiredHeadphoneMode = false;

	//////////////////////////////////////////////////////////////////
	// 音频输出设备
	// 只记录玩家的选择，设备切换本身由 UYanSettingValueDiscreteDynamic_AudioOutputDevice 决定是否执行

public:
	/** 取玩家选定的输出设备 Id；为空表示跟随系统默认设备 */
	UFUNCTION()
	FString GetAudioOutputDeviceId() const { return AudioOutputDeviceId; }

	UFUNCTION()
	void SetAudioOutputDeviceId(const FString& InAudioOutputDeviceId) { AudioOutputDeviceId = InAudioOutputDeviceId; }

public:
	/** 是否在对局中自动录制客户端回放。 */
	UFUNCTION()
	bool ShouldAutoRecordReplays() const { return bShouldAutoRecordReplays; }

	UFUNCTION()
	void SetShouldAutoRecordReplays(bool bEnabled) { bShouldAutoRecordReplays = bEnabled; }

	/** 本地保留的回放份数，超出的从最旧一份开始清理。 */
	UFUNCTION()
	int32 GetNumberOfReplaysToKeep() const { return NumberOfReplaysToKeep; }

	UFUNCTION()
	void SetNumberOfReplaysToKeep(int32 InNumberOfReplays) { NumberOfReplaysToKeep = InNumberOfReplays; }

private:
	// 加载音量总线与用户混音，并把当前数值推送上去
	void LoadUserControlBusMix();
	void SetVolumeForControlBus(USoundControlBus* InSoundControlBus, float InVolume);

	UPROPERTY(Config)
	float OverallVolume = 1.0f;

	UPROPERTY(Config)
	float MusicVolume = 1.0f;

	UPROPERTY(Config)
	float SoundFXVolume = 1.0f;

	UPROPERTY(Config)
	float DialogueVolume = 1.0f;

	UPROPERTY(Config)
	float VoiceChatVolume = 1.0f;

	UPROPERTY(Config)
	bool bUseHDRAudioMode = false;

	UPROPERTY(Config)
	bool bShouldAutoRecordReplays = false;

	UPROPERTY(Config)
	int32 NumberOfReplaysToKeep = 5;

	// 双耳渲染的生效值，与 bDesiredHeadphoneMode 可能短暂不一致
	UPROPERTY(Config)
	bool bUseHeadphoneMode = false;

	UPROPERTY(Config)
	FString AudioOutputDeviceId;

	// 按用途索引的控制总线缓存
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USoundControlBus>> ControlBusMap;

	UPROPERTY(Transient)
	TObjectPtr<USoundControlBusMix> ControlBusMix = nullptr;

	bool bSoundControlBusMixLoaded = false;

	//////////////////////////////////////////////////////////////////
	// 性能统计显示
	// 仅记录玩家选择的显示方式，数值本身由 UYanPerformanceStatSubsystem 采集

public:
	/** 取某项性能统计的显示方式，未设置过则为隐藏 */
	UE_API EYanStatDisplayMode GetPerfStatDisplayState(EYanDisplayablePerformanceStat Stat) const;

	/** 设置某项性能统计的显示方式 */
	UE_API void SetPerfStatDisplayState(EYanDisplayablePerformanceStat Stat, EYanStatDisplayMode DisplayMode);

	DECLARE_EVENT(UYanSettingsLocal, FPerfStatSettingsChanged);

	/** 显示方式变化或设置被应用时触发，供 HUD 上的统计容器订阅 */
	FPerfStatSettingsChanged& OnPerfStatDisplayStateChanged() { return PerfStatSettingsChangedEvent; }

private:
	// 需要显示在 HUD 上的统计项；隐藏项直接移除而非存为 Hidden
	UPROPERTY(Config)
	TMap<EYanDisplayablePerformanceStat, EYanStatDisplayMode> DisplayStatList;

	FPerfStatSettingsChanged PerfStatSettingsChangedEvent;

	//////////////////////////////////////////////////////////////////
	// 延迟统计
	// 数据由 ILatencyMarkerModule 的实现（如 Reflex）提供，
	// 平台是否具备该能力经 CommonUI 的平台特性标签描述

public:
	/** 平台是否支持采集延迟统计 */
	static UE_API bool DoesPlatformSupportLatencyTrackingStats();

	DECLARE_EVENT(UYanSettingsLocal, FLatencyStatEnabledSettingChanged);

	/** 延迟统计开关变化时触发，供设置项刷新可编辑状态 */
	FLatencyStatEnabledSettingChanged& OnLatencyStatIndicatorSettingsChangedEvent() { return LatencyStatIndicatorSettingsChangedEvent; }

	UFUNCTION()
	UE_API void SetEnableLatencyTrackingStats(const bool bNewVal);

	UFUNCTION()
	bool GetEnableLatencyTrackingStats() const { return bEnableLatencyTrackingStats; }

	/** 平台是否支持延迟闪烁标记 */
	static UE_API bool DoesPlatformSupportLatencyMarkers();

	DECLARE_EVENT(UYanSettingsLocal, FLatencyFlashIndicatorSettingChanged);

	/** 闪烁标记开关变化时触发，供输入侧决定是否插入标记 */
	FLatencyFlashIndicatorSettingChanged& OnLatencyFlashIndicatorSettingsChangedEvent() { return LatencyFlashIndicatorSettingsChangedEvent; }

	UFUNCTION()
	UE_API void SetEnableLatencyFlashIndicators(const bool bNewVal);

	UFUNCTION()
	bool GetEnableLatencyFlashIndicators() const { return bEnableLatencyFlashIndicators; }

private:
	// 把当前开关同步到所有延迟标记模块
	void ApplyLatencyTrackingStatSetting();

	// 默认值取决于平台是否支持，故由 SetToDefaults 决定而非在此初始化
	UPROPERTY(Config)
	bool bEnableLatencyTrackingStats;

	// 屏幕闪烁标记用于外部测量端到端延迟，默认关闭
	UPROPERTY(Config)
	bool bEnableLatencyFlashIndicators = false;

	FLatencyStatEnabledSettingChanged LatencyStatIndicatorSettingsChangedEvent;

	FLatencyFlashIndicatorSettingChanged LatencyFlashIndicatorSettingsChangedEvent;

private:
	void OnAppActivationStateChanged(bool bIsActive);

	FDelegateHandle OnApplicationActivationStateChangedHandle;
};

#undef UE_API
