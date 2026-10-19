#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "Engine/PlatformSettings.h"
#include "GameplayTagContainer.h"

#include "YanPerformanceSettings.generated.h"

#define UE_API YANGAMESETTING_API

enum class EYanDisplayablePerformanceStat : uint8;

/** 一个可供玩家在设置界面选择的平台设备配置变体。 */
USTRUCT()
struct FYanQualityDeviceProfileVariant
{
	GENERATED_BODY()

	// 在设置界面显示的名称
	UPROPERTY(EditAnywhere)
	FText DisplayName;

	// 追加到当前平台基础设备配置名之后的后缀
	UPROPERTY(EditAnywhere)
	FString DeviceProfileSuffix;

	// 启用该模式所需的最低刷新率；显示器达不到时该项不可选
	UPROPERTY(EditAnywhere)
	int32 MinRefreshRate = 0;
};

/** 一组性能统计项，是否可选由平台特性查询决定。 */
USTRUCT()
struct FYanPerformanceStatGroup
{
	GENERATED_BODY()

	// 平台特性查询，决定这组统计项能否显示
	UPROPERTY(EditAnywhere, meta = (Categories = "Input,Platform.Trait"))
	FGameplayTagQuery VisibilityQuery;

	// 查询通过时允许显示的统计项
	UPROPERTY(EditAnywhere)
	TSet<EYanDisplayablePerformanceStat> AllowedStats;
};

/** 平台如何控制帧率节奏与整体画质设置。 */
UENUM()
enum class EYanFramePacingMode : uint8
{
	// 手动帧率上限，玩家可自行决定是否锁垂直同步
	DesktopStyle,

	// 由设备配置驱动的呈现间隔控制
	ConsoleStyle,

	// 玩家从设备配置允许的帧率中挑选
	MobileStyle
};

/**
 * 分平台的渲染与帧率设置。
 * 实际取值通常来自各平台的 ini，而非在界面中编辑。
 */
UCLASS(MinimalAPI, config = Game, defaultconfig)
class UYanPlatformSpecificRenderingSettings : public UPlatformSettings
{
	GENERATED_BODY()

public:
	UE_API UYanPlatformSpecificRenderingSettings();

	/** 取当前平台对应的设置实例。 */
	static UE_API const UYanPlatformSpecificRenderingSettings* Get();

	// 默认追加的变体后缀，一般应是 UserFacingDeviceProfileOptions 中的一项
	UPROPERTY(EditAnywhere, Config, Category = DeviceProfiles)
	FString DefaultDeviceProfileSuffix;

	// 供玩家选择的设备配置变体，须按目标帧率从低到高排列：
	// 当前显示器不支持所选刷新率时会依次回退到前一项
	UPROPERTY(EditAnywhere, Config, Category = DeviceProfiles)
	TArray<FYanQualityDeviceProfileVariant> UserFacingDeviceProfileOptions;

	// 平台是否支持细分的画质设置
	UPROPERTY(EditAnywhere, Config, Category = VideoSettings)
	bool bSupportsGranularVideoQualitySettings = true;

	// 平台是否支持自动画质基准测试，通常仅在支持细分画质时才为真
	UPROPERTY(EditAnywhere, Config, Category = VideoSettings)
	bool bSupportsAutomaticVideoQualityBenchmark = true;

	// 帧率节奏的控制方式
	UPROPERTY(EditAnywhere, Config, Category = VideoSettings)
	EYanFramePacingMode FramePacingMode = EYanFramePacingMode::DesktopStyle;

	// 移动端可选帧率，进一步受设备配置与平台帧率控制器实际支持情况限制
	UPROPERTY(EditAnywhere, Config, Category = VideoSettings, meta = (EditCondition = "FramePacingMode==EYanFramePacingMode::MobileStyle", ForceUnits = Hz))
	TArray<int32> MobileFrameRateLimits;
};

/**
 * 项目级性能配置。
 */
UCLASS(MinimalAPI, config = Game, defaultconfig, meta = (DisplayName = "Yan Performance Settings"))
class UYanPerformanceSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()

public:
	UE_API UYanPerformanceSettings();

	// 桌面平台上「帧率上限」设置项可供玩家选择的帧率
	UPROPERTY(EditAnywhere, Config, Category = Performance, meta = (ForceUnits = Hz))
	TArray<int32> DesktopFrameRateLimits;

	// 玩家可在设置中开启的性能统计项
	UPROPERTY(EditAnywhere, Config, Category = Stats)
	TArray<FYanPerformanceStatGroup> UserFacingPerformanceStats;

private:
	// 仅用于把分平台设置暴露到项目设置界面，无需直接访问
	UPROPERTY(EditAnywhere, Category = "PlatformSpecific")
	FPerPlatformSettings PerPlatformSettings;
};

#undef UE_API
