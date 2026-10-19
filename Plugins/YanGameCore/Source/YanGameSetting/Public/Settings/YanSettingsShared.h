// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/SaveGame.h"
#include "SubtitleDisplayOptions.h"

#include "YanSettingsShared.generated.h"

#define UE_API YANGAMESETTING_API

class ULocalPlayer;

/** 游戏切至后台时的音频策略。 */
UENUM(BlueprintType)
enum class EYanAllowBackgroundAudioSetting : uint8
{
	Off,
	AllSounds,

	Num UMETA(Hidden),
};

/** 手柄视角灵敏度档位，具体倍率由 UYanAimSensitivityData 映射。 */
UENUM(BlueprintType)
enum class EYanGamepadSensitivity : uint8
{
	Invalid = 0 UMETA(Hidden),

	Slow           UMETA(DisplayName = "01 - 慢"),
	SlowPlus       UMETA(DisplayName = "02 - 慢+"),
	SlowPlusPlus   UMETA(DisplayName = "03 - 慢++"),
	Normal         UMETA(DisplayName = "04 - 普通"),
	NormalPlus     UMETA(DisplayName = "05 - 普通+"),
	NormalPlusPlus UMETA(DisplayName = "06 - 普通++"),
	Fast           UMETA(DisplayName = "07 - 快"),
	FastPlus       UMETA(DisplayName = "08 - 快+"),
	FastPlusPlus   UMETA(DisplayName = "09 - 快++"),
	Insane         UMETA(DisplayName = "10 - 极快"),

	MAX UMETA(Hidden),
};

/**
 * 跨设备共享的玩家偏好，经存档系统按玩家保存，可安全上云。
 * 与机器相关的画面设置见 UYanSettingsLocal；
 * 键位映射本身由 UEnhancedInputUserSettings 持有，此处只负责与之同期存盘。
 * 实例由 UYanSettingsSubsystem 持有。
 */
UCLASS(MinimalAPI)
class UYanSettingsShared : public ULocalPlayerSaveGame
{
	GENERATED_BODY()

public:
	DECLARE_EVENT_OneParam(UYanSettingsShared, FOnSettingChangedEvent, UYanSettingsShared* Settings);
	FOnSettingChangedEvent OnSettingChanged;

public:
	//~Begin ULocalPlayerSaveGame Interface
	UE_API virtual int32 GetLatestDataVersion() const override;
	//~End ULocalPlayerSaveGame Interface

	bool IsDirty() const { return bIsDirty; }
	void ClearDirtyFlag() { bIsDirty = false; }

	/** 创建临时设置对象，随后会被玩家存档中加载出的实例替换 */
	static UE_API UYanSettingsShared* CreateTemporarySettings(const ULocalPlayer* LocalPlayer);

	/** 同步加载，登录完成前调用无效 */
	static UE_API UYanSettingsShared* LoadOrCreateSettings(const ULocalPlayer* LocalPlayer);

	DECLARE_DELEGATE_OneParam(FOnSettingsLoadedEvent, UYanSettingsShared* Settings);

	/** 异步加载设置对象，完成后回调 Delegate */
	static UE_API bool AsyncLoadOrCreateSettings(const ULocalPlayer* LocalPlayer, FOnSettingsLoadedEvent Delegate);

	/** 存盘，同时触发键位映射的异步保存 */
	UE_API void SaveSettings();

	/** 把当前设置应用到玩家 */
	UE_API void ApplySettings();

	//////////////////////////////////////////////////////////////////
	// 鼠标灵敏度
	// 数值本身在此存储，实际缩放由输入 Modifier 每帧读取后施加

public:
	UFUNCTION()
	double GetMouseSensitivityX() const { return MouseSensitivityX; }

	UFUNCTION()
	void SetMouseSensitivityX(double NewValue) { ChangeValueAndDirty(MouseSensitivityX, NewValue); }

	UFUNCTION()
	double GetMouseSensitivityY() const { return MouseSensitivityY; }

	UFUNCTION()
	void SetMouseSensitivityY(double NewValue) { ChangeValueAndDirty(MouseSensitivityY, NewValue); }

	UFUNCTION()
	double GetTargetingMultiplier() const { return TargetingMultiplier; }

	UFUNCTION()
	void SetTargetingMultiplier(double NewValue) { ChangeValueAndDirty(TargetingMultiplier, NewValue); }

	UFUNCTION()
	bool GetInvertVerticalAxis() const { return bInvertVerticalAxis; }

	UFUNCTION()
	void SetInvertVerticalAxis(bool NewValue) { ChangeValueAndDirty(bInvertVerticalAxis, NewValue); }

	UFUNCTION()
	bool GetInvertHorizontalAxis() const { return bInvertHorizontalAxis; }

	UFUNCTION()
	void SetInvertHorizontalAxis(bool NewValue) { ChangeValueAndDirty(bInvertHorizontalAxis, NewValue); }

private:
	/** 鼠标水平灵敏度 */
	UPROPERTY()
	double MouseSensitivityX = 1.0;

	/** 鼠标垂直灵敏度 */
	UPROPERTY()
	double MouseSensitivityY = 1.0;

	/** 瞄准状态下叠加的灵敏度倍率 */
	UPROPERTY()
	double TargetingMultiplier = 0.5;

	/** 反转垂直视角轴 */
	UPROPERTY()
	bool bInvertVerticalAxis = false;

	/** 反转水平视角轴 */
	UPROPERTY()
	bool bInvertHorizontalAxis = false;

	//////////////////////////////////////////////////////////////////
	// 手柄灵敏度与摇杆死区
	// 灵敏度只存档位，实际倍率由 UYanAimSensitivityData 查表得到，
	// 使手感曲线可在数据资产中调整而不影响存档

public:
	UFUNCTION()
	EYanGamepadSensitivity GetGamepadLookSensitivityPreset() const { return GamepadLookSensitivityPreset; }

	UFUNCTION()
	void SetGamepadLookSensitivityPreset(EYanGamepadSensitivity NewValue) { ChangeValueAndDirty(GamepadLookSensitivityPreset, NewValue); }

	UFUNCTION()
	EYanGamepadSensitivity GetGamepadTargetingSensitivityPreset() const { return GamepadTargetingSensitivityPreset; }

	UFUNCTION()
	void SetGamepadTargetingSensitivityPreset(EYanGamepadSensitivity NewValue) { ChangeValueAndDirty(GamepadTargetingSensitivityPreset, NewValue); }

	UFUNCTION()
	double GetGamepadMoveStickDeadZone() const { return GamepadMoveStickDeadZone; }

	UFUNCTION()
	void SetGamepadMoveStickDeadZone(double NewValue) { ChangeValueAndDirty(GamepadMoveStickDeadZone, NewValue); }

	UFUNCTION()
	double GetGamepadLookStickDeadZone() const { return GamepadLookStickDeadZone; }

	UFUNCTION()
	void SetGamepadLookStickDeadZone(double NewValue) { ChangeValueAndDirty(GamepadLookStickDeadZone, NewValue); }

private:
	/** 常规视角的手柄灵敏度档位 */
	UPROPERTY()
	EYanGamepadSensitivity GamepadLookSensitivityPreset = EYanGamepadSensitivity::Normal;

	/** 瞄准状态下的手柄灵敏度档位 */
	UPROPERTY()
	EYanGamepadSensitivity GamepadTargetingSensitivityPreset = EYanGamepadSensitivity::Normal;

	/** 移动摇杆死区 */
	UPROPERTY()
	double GamepadMoveStickDeadZone = 0.25;

	/** 视角摇杆死区 */
	UPROPERTY()
	double GamepadLookStickDeadZone = 0.25;

	//////////////////////////////////////////////////////////////////
	// 语言 / 地区
	// 待应用的语言先记在 PendingCulture，直到 ApplyCultureSettings 才真正切换，
	// 因为切换需要重启才能完全生效，UI 需要在应用前展示待生效的选择

public:
	/** 取当前待应用的语言；为空表示无待应用变更 */
	UE_API const FString& GetPendingCulture() const;

	/** 记录待应用的语言 */
	UE_API void SetPendingCulture(const FString& NewCulture);

	/** 语言已在别处切换时调用，用于丢弃本地的待应用状态 */
	UE_API void OnCultureChanged();

	/** 清除待应用的语言 */
	UE_API void ClearPendingCulture();

	/** 是否正在使用系统默认语言（即用户配置中未写死 Culture） */
	UE_API bool IsUsingDefaultCulture() const;

	/** 标记为下次应用时回到系统默认语言 */
	UE_API void ResetToDefaultCulture();

	bool ShouldResetToDefaultCulture() const { return bResetToDefaultCulture; }

	/** 将待应用的语言真正切换到国际化系统，并写入用户配置 */
	UE_API void ApplyCultureSettings();

	/** 丢弃尚未应用的语言变更，回到当前生效的设置 */
	UE_API void ResetCultureToCurrentSettings();

private:
	/** 待应用的语言，不入存档：真正的持久化由 GGameUserSettingsIni 承担 */
	UPROPERTY(Transient)
	FString PendingCulture;

	// 为真时下次应用回到系统默认语言
	bool bResetToDefaultCulture = false;

	//////////////////////////////////////////////////////////////////
	// 字幕
	// 外观参数存于此处，实际排版由 USubtitleDisplaySubsystem 按格式重建，
	// 因此每次修改都需立即推送，界面才能即时预览

public:
	UFUNCTION()
	bool GetSubtitlesEnabled() const { return bEnableSubtitles; }

	UFUNCTION()
	void SetSubtitlesEnabled(bool Value) { ChangeValueAndDirty(bEnableSubtitles, Value); }

	UFUNCTION()
	ESubtitleDisplayTextSize GetSubtitlesTextSize() const { return SubtitleTextSize; }

	UFUNCTION()
	void SetSubtitlesTextSize(ESubtitleDisplayTextSize Value) { ChangeValueAndDirty(SubtitleTextSize, Value); ApplySubtitleOptions(); }

	UFUNCTION()
	ESubtitleDisplayTextColor GetSubtitlesTextColor() const { return SubtitleTextColor; }

	UFUNCTION()
	void SetSubtitlesTextColor(ESubtitleDisplayTextColor Value) { ChangeValueAndDirty(SubtitleTextColor, Value); ApplySubtitleOptions(); }

	UFUNCTION()
	ESubtitleDisplayTextBorder GetSubtitlesTextBorder() const { return SubtitleTextBorder; }

	UFUNCTION()
	void SetSubtitlesTextBorder(ESubtitleDisplayTextBorder Value) { ChangeValueAndDirty(SubtitleTextBorder, Value); ApplySubtitleOptions(); }

	UFUNCTION()
	ESubtitleDisplayBackgroundOpacity GetSubtitlesBackgroundOpacity() const { return SubtitleBackgroundOpacity; }

	UFUNCTION()
	void SetSubtitlesBackgroundOpacity(ESubtitleDisplayBackgroundOpacity Value) { ChangeValueAndDirty(SubtitleBackgroundOpacity, Value); ApplySubtitleOptions(); }

	/** 将当前字幕外观推送给字幕子系统 */
	UE_API void ApplySubtitleOptions();

private:
	UPROPERTY()
	bool bEnableSubtitles = true;

	UPROPERTY()
	ESubtitleDisplayTextSize SubtitleTextSize = ESubtitleDisplayTextSize::Medium;

	UPROPERTY()
	ESubtitleDisplayTextColor SubtitleTextColor = ESubtitleDisplayTextColor::White;

	UPROPERTY()
	ESubtitleDisplayTextBorder SubtitleTextBorder = ESubtitleDisplayTextBorder::None;

	UPROPERTY()
	ESubtitleDisplayBackgroundOpacity SubtitleBackgroundOpacity = ESubtitleDisplayBackgroundOpacity::Medium;

	//////////////////////////////////////////////////////////////////
	// 后台音频

public:
	UFUNCTION()
	EYanAllowBackgroundAudioSetting GetAllowAudioInBackgroundSetting() const { return AllowAudioInBackground; }

	UFUNCTION()
	UE_API void SetAllowAudioInBackgroundSetting(EYanAllowBackgroundAudioSetting NewValue);

	/** 按当前设置调整失焦时的音量倍率 */
	UE_API void ApplyBackgroundAudioSettings();

private:
	UPROPERTY()
	EYanAllowBackgroundAudioSetting AllowAudioInBackground = EYanAllowBackgroundAudioSetting::Off;

	//////////////////////////////////////////////////////////////////
	// 变更追踪

private:
	template<typename T>
	bool ChangeValueAndDirty(T& CurrentValue, const T& NewValue)
	{
		if (CurrentValue != NewValue)
		{
			CurrentValue = NewValue;
			bIsDirty = true;
			OnSettingChanged.Broadcast(this);

			return true;
		}

		return false;
	}

	bool bIsDirty = false;
};

#undef UE_API
