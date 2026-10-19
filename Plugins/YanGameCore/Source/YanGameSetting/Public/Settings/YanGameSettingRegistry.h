// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataSource/GameSettingDataSourceDynamic.h" // IWYU pragma: keep
#include "GameSettingRegistry.h"
#include "Settings/YanSettingsLocal.h" // IWYU pragma: keep

#include "YanGameSettingRegistry.generated.h"

#define UE_API YANGAMESETTING_API

class ULocalPlayer;
class UGameSettingCollection;

YANGAMESETTING_API DECLARE_LOG_CATEGORY_EXTERN(LogYanGameSettingRegistry, Log, Log);

// 属性路径以 ULocalPlayer 为根解析，第一跳必须是 LocalPlayer 子类上的 UFUNCTION。
// 本模块不依赖具体的 LocalPlayer 子类，故第一跳只能写成字面量；名称若与
// UYanLocalPlayer 上的访问器不一致，运行时取值会 ensure 失败，可用 Yan.Settings.Dump 排查。
#define YAN_LOCAL_PLAYER_SHARED_SETTINGS_GETTER TEXT("GetSharedSettings")
#define YAN_LOCAL_PLAYER_LOCAL_SETTINGS_GETTER TEXT("GetLocalSettings")

// 设置项经属性路径而非直接指针访问设置对象，因此共享设置在登录后被替换也不会失效
#define GET_SHARED_SETTINGS_FUNCTION_PATH(FunctionOrPropertyName)							\
	MakeShared<FGameSettingDataSourceDynamic>(TArray<FString>({								\
		YAN_LOCAL_PLAYER_SHARED_SETTINGS_GETTER,											\
		GET_FUNCTION_NAME_STRING_CHECKED(UYanSettingsShared, FunctionOrPropertyName)			\
	}))

#define GET_LOCAL_SETTINGS_FUNCTION_PATH(FunctionOrPropertyName)							\
	MakeShared<FGameSettingDataSourceDynamic>(TArray<FString>({								\
		YAN_LOCAL_PLAYER_LOCAL_SETTINGS_GETTER,												\
		GET_FUNCTION_NAME_STRING_CHECKED(UYanSettingsLocal, FunctionOrPropertyName)			\
	}))

/**
 * 玩家设置树的根节点，按本地玩家实例化。
 * 只描述设置的结构与取值范围，实际数据落在 UYanSettingsLocal 与 UYanSettingsShared 上。
 */
UCLASS(MinimalAPI)
class UYanGameSettingRegistry : public UGameSettingRegistry
{
	GENERATED_BODY()

public:
	static UE_API UYanGameSettingRegistry* Get(ULocalPlayer* InLocalPlayer);

	UE_API virtual void SaveChanges() override;

#if !UE_BUILD_SHIPPING
	/** 经控制台命令 Yan.Settings.Dump 打印设置树 */
	UE_API void DumpSettings();
#endif

protected:
	UE_API virtual void OnInitialize(ULocalPlayer* InLocalPlayer) override;
	UE_API virtual bool IsFinishedInitializing() const override;

	UGameSettingCollection* InitializeVideoSettings(ULocalPlayer* InLocalPlayer);
	void InitializeVideoSettings_FrameRates(UGameSettingCollection* Screen, ULocalPlayer* InLocalPlayer);

	// 性能统计以子页面形式挂在显示分类下，页面内的统计项直接写入 UYanSettingsLocal
	void AddPerformanceStatPage(UGameSettingCollection* PerfStatsOuterCategory, ULocalPlayer* InLocalPlayer);

	UGameSettingCollection* InitializeMouseAndKeyboardSettings(ULocalPlayer* InLocalPlayer);

	UGameSettingCollection* InitializeAudioSettings(ULocalPlayer* InLocalPlayer);

	// 游戏分类：语言与回放录制两个子节；语言写入共享设置的 PendingCulture，回放写入本地设置
	UGameSettingCollection* InitializeGameplaySettings(ULocalPlayer* InLocalPlayer);

	// 注册一个尚无设置项的分类。设置界面按 DevName 查询分类，查不到会触发断言，
	// 因此未实现的分类也需占位；界面依据分类是否为空决定页签显隐
	void RegisterEmptyCollection(ULocalPlayer* InLocalPlayer, FName DevName, const FText& DisplayName);

	UPROPERTY()
	TObjectPtr<UGameSettingCollection> VideoSettings;

	UPROPERTY()
	TObjectPtr<UGameSettingCollection> MouseAndKeyboardSettings;

	UPROPERTY()
	TObjectPtr<UGameSettingCollection> AudioSettings;

	UPROPERTY()
	TObjectPtr<UGameSettingCollection> GameplaySettings;
};

#undef UE_API
