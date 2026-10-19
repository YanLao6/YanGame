// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditCondition/WhenPlayingAsPrimaryPlayer.h"
#include "GameSettingCollection.h"
#include "GameSettingValueDiscreteDynamic.h"
#include "HAL/IConsoleManager.h"
#include "Performance/YanPerformanceStatTypes.h"
#include "RHI.h"
#include "Settings/CustomSettings/YanSettingValueDiscrete_PerfStat.h"
#include "Settings/YanGameSettingRegistry.h"
#include "Settings/YanSettingsLocal.h"

#define LOCTEXT_NAMESPACE "Yan"

static TAutoConsoleVariable<bool> CVarLatencyMarkersRequireNVIDIA(TEXT("Yan.Settings.LatencyMarkersRequireNVIDIA"),
	true,
	TEXT("延迟标记是否仅在 NVIDIA 硬件上可用"),
	ECVF_Default);

// 平台是否具备采集延迟统计的能力
class FGameSettingEditCondition_LatencyStatsSupported final : public FGameSettingEditCondition
{
public:
	FGameSettingEditCondition_LatencyStatsSupported() = default;

	//~Begin FGameSettingEditCondition Interface
	virtual void GatherEditState(const ULocalPlayer* InLocalPlayer, FGameSettingEditableState& InOutEditState) const override
	{
		if (!UYanSettingsLocal::DoesPlatformSupportLatencyTrackingStats())
		{
			InOutEditState.Disable(LOCTEXT("PlatformDoesNotSupportLatencyStats", "当前设备不支持延迟统计。"));
		}
	}
	//~End FGameSettingEditCondition Interface
};

// 延迟数据只在统计开关打开时才有值，故依赖项需随该开关实时刷新
class FGameSettingEditCondition_LatencyStatsCurrentlyEnabled final : public FGameSettingEditCondition
{
public:
	FGameSettingEditCondition_LatencyStatsCurrentlyEnabled() = default;

	virtual ~FGameSettingEditCondition_LatencyStatsCurrentlyEnabled() override
	{
		if (!SettingChangedDelegate.IsValid())
		{
			return;
		}

		if (UYanSettingsLocal* Settings = UYanSettingsLocal::Get())
		{
			Settings->OnLatencyStatIndicatorSettingsChangedEvent().Remove(SettingChangedDelegate);
		}
	}

private:
	//~Begin FGameSettingEditCondition Interface
	virtual void Initialize(const ULocalPlayer* InLocalPlayer) override
	{
		UYanSettingsLocal* Settings = UYanSettingsLocal::Get();
		if (!Settings)
		{
			return;
		}

		SettingChangedDelegate = Settings->OnLatencyStatIndicatorSettingsChangedEvent().AddSP(
			this->AsShared(), &FGameSettingEditCondition_LatencyStatsCurrentlyEnabled::BroadcastEditConditionChanged);
	}

	virtual void GatherEditState(const ULocalPlayer* InLocalPlayer, FGameSettingEditableState& InOutEditState) const override
	{
		const UYanSettingsLocal* Settings = UYanSettingsLocal::Get();
		if (!Settings)
		{
			return;
		}

		if (!Settings->GetEnableLatencyTrackingStats())
		{
			InOutEditState.Disable(LOCTEXT("LatencyMarkerRequireStatsEnabled", "需要先开启延迟统计。"));
		}
	}
	//~End FGameSettingEditCondition Interface

	FDelegateHandle SettingChangedDelegate;
};

// 延迟标记依赖具体厂商的实现，能力与硬件均需满足
class FGameSettingEditCondition_LatencyMarkersSupported final : public FGameSettingEditCondition
{
public:
	FGameSettingEditCondition_LatencyMarkersSupported() = default;

	//~Begin FGameSettingEditCondition Interface
	virtual void GatherEditState(const ULocalPlayer* InLocalPlayer, FGameSettingEditableState& InOutEditState) const override
	{
		if (!UYanSettingsLocal::DoesPlatformSupportLatencyMarkers())
		{
			InOutEditState.Disable(LOCTEXT("PlatformDoesNotSupportLatencyMarkers", "当前设备不支持延迟标记。"));
		}

		// 目前只有 Reflex 一种延迟标记实现，其余硬件上开启没有意义
		if (CVarLatencyMarkersRequireNVIDIA.GetValueOnAnyThread() && !IsRHIDeviceNVIDIA())
		{
			InOutEditState.Disable(LOCTEXT("InputLatencyMarkersRequiresNVIDIA", "延迟标记仅在 NVIDIA 显卡上可用。"));
		}
	}
	//~End FGameSettingEditCondition Interface
};

//////////////////////////////////////////////////////////////////

void UYanGameSettingRegistry::AddPerformanceStatPage(UGameSettingCollection* PerfStatsOuterCategory, ULocalPlayer* InLocalPlayer)
{
	static_assert(static_cast<int32>(EYanDisplayablePerformanceStat::Count) == 19, "新增性能统计项后需同步更新本页面");

	UGameSettingCollectionPage* StatsPage = NewObject<UGameSettingCollectionPage>();
	StatsPage->SetDevName(TEXT("PerfStatsPage"));
	StatsPage->SetDisplayName(LOCTEXT("PerfStatsPage_Name", "性能统计"));
	StatsPage->SetDescriptionRichText(LOCTEXT("PerfStatsPage_Description", "选择需要显示在屏幕上的性能统计数据。"));
	StatsPage->SetNavigationText(LOCTEXT("PerfStatsPage_Navigation", "编辑"));

	// 统计显示是机器本地设置，分屏时只对主玩家开放
	StatsPage->AddEditCondition(FWhenPlayingAsPrimaryPlayer::Get());

	PerfStatsOuterCategory->AddSetting(StatsPage);

	// 性能
	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* StatCategory_Performance = NewObject<UGameSettingCollection>();
		StatCategory_Performance->SetDevName(TEXT("StatCategory_Performance"));
		StatCategory_Performance->SetDisplayName(LOCTEXT("StatCategory_Performance_Name", "性能"));
		StatsPage->AddSetting(StatCategory_Performance);

		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_PerfStat* Setting = NewObject<UYanSettingValueDiscrete_PerfStat>();
			Setting->SetStat(EYanDisplayablePerformanceStat::ClientFPS);
			Setting->SetDisplayName(LOCTEXT("PerfStat_ClientFPS", "客户端帧率"));
			Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_ClientFPS", "本机每秒渲染的帧数，越高越好。"));
			StatCategory_Performance->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_PerfStat* Setting = NewObject<UYanSettingValueDiscrete_PerfStat>();
			Setting->SetStat(EYanDisplayablePerformanceStat::ServerFPS);
			Setting->SetDisplayName(LOCTEXT("PerfStat_ServerFPS", "服务器帧率"));
			Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_ServerFPS", "服务器每秒的模拟次数。"));
			StatCategory_Performance->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_PerfStat* Setting = NewObject<UYanSettingValueDiscrete_PerfStat>();
			Setting->SetStat(EYanDisplayablePerformanceStat::FrameTime);
			Setting->SetDisplayName(LOCTEXT("PerfStat_FrameTime", "整帧耗时"));
			Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_FrameTime", "完成一帧所需的总时间。"));
			StatCategory_Performance->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_PerfStat* Setting = NewObject<UYanSettingValueDiscrete_PerfStat>();
			Setting->SetStat(EYanDisplayablePerformanceStat::IdleTime);
			Setting->SetDisplayName(LOCTEXT("PerfStat_IdleTime", "空闲耗时"));
			Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_IdleTime", "为控制帧率而等待的时间。"));
			StatCategory_Performance->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_PerfStat* Setting = NewObject<UYanSettingValueDiscrete_PerfStat>();
			Setting->SetStat(EYanDisplayablePerformanceStat::FrameTime_GameThread);
			Setting->SetDisplayName(LOCTEXT("PerfStat_FrameTime_GameThread", "游戏线程耗时"));
			Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_FrameTime_GameThread", "主游戏线程上花费的时间。"));
			StatCategory_Performance->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_PerfStat* Setting = NewObject<UYanSettingValueDiscrete_PerfStat>();
			Setting->SetStat(EYanDisplayablePerformanceStat::FrameTime_RenderThread);
			Setting->SetDisplayName(LOCTEXT("PerfStat_FrameTime_RenderThread", "渲染线程耗时"));
			Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_FrameTime_RenderThread", "渲染线程上花费的时间。"));
			StatCategory_Performance->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_PerfStat* Setting = NewObject<UYanSettingValueDiscrete_PerfStat>();
			Setting->SetStat(EYanDisplayablePerformanceStat::FrameTime_RHIThread);
			Setting->SetDisplayName(LOCTEXT("PerfStat_FrameTime_RHIThread", "RHI 线程耗时"));
			Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_FrameTime_RHIThread", "向图形驱动提交指令所花费的时间。"));
			StatCategory_Performance->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_PerfStat* Setting = NewObject<UYanSettingValueDiscrete_PerfStat>();
			Setting->SetStat(EYanDisplayablePerformanceStat::FrameTime_GPU);
			Setting->SetDisplayName(LOCTEXT("PerfStat_FrameTime_GPU", "GPU 耗时"));
			Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_FrameTime_GPU", "显卡完成一帧渲染所花费的时间。"));
			StatCategory_Performance->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
	}

	// 网络
	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* StatCategory_Network = NewObject<UGameSettingCollection>();
		StatCategory_Network->SetDevName(TEXT("StatCategory_Network"));
		StatCategory_Network->SetDisplayName(LOCTEXT("StatCategory_Network_Name", "网络"));
		StatsPage->AddSetting(StatCategory_Network);

		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_PerfStat* Setting = NewObject<UYanSettingValueDiscrete_PerfStat>();
			Setting->SetStat(EYanDisplayablePerformanceStat::Ping);
			Setting->SetDisplayName(LOCTEXT("PerfStat_Ping", "延迟"));
			Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_Ping", "与服务器往返一次所需的时间。"));
			StatCategory_Network->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_PerfStat* Setting = NewObject<UYanSettingValueDiscrete_PerfStat>();
			Setting->SetStat(EYanDisplayablePerformanceStat::PacketLoss_Incoming);
			Setting->SetDisplayName(LOCTEXT("PerfStat_PacketLoss_Incoming", "入向丢包率"));
			Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_PacketLoss_Incoming", "接收数据包的丢失比例。"));
			StatCategory_Network->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_PerfStat* Setting = NewObject<UYanSettingValueDiscrete_PerfStat>();
			Setting->SetStat(EYanDisplayablePerformanceStat::PacketLoss_Outgoing);
			Setting->SetDisplayName(LOCTEXT("PerfStat_PacketLoss_Outgoing", "出向丢包率"));
			Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_PacketLoss_Outgoing", "发送数据包的丢失比例。"));
			StatCategory_Network->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_PerfStat* Setting = NewObject<UYanSettingValueDiscrete_PerfStat>();
			Setting->SetStat(EYanDisplayablePerformanceStat::PacketRate_Incoming);
			Setting->SetDisplayName(LOCTEXT("PerfStat_PacketRate_Incoming", "入向包速率"));
			Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_PacketRate_Incoming", "每秒接收的数据包数量。"));
			StatCategory_Network->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_PerfStat* Setting = NewObject<UYanSettingValueDiscrete_PerfStat>();
			Setting->SetStat(EYanDisplayablePerformanceStat::PacketRate_Outgoing);
			Setting->SetDisplayName(LOCTEXT("PerfStat_PacketRate_Outgoing", "出向包速率"));
			Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_PacketRate_Outgoing", "每秒发送的数据包数量。"));
			StatCategory_Network->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_PerfStat* Setting = NewObject<UYanSettingValueDiscrete_PerfStat>();
			Setting->SetStat(EYanDisplayablePerformanceStat::PacketSize_Incoming);
			Setting->SetDisplayName(LOCTEXT("PerfStat_PacketSize_Incoming", "入向包大小"));
			Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_PacketSize_Incoming", "最近一秒收到的数据包平均字节数。"));
			StatCategory_Network->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_PerfStat* Setting = NewObject<UYanSettingValueDiscrete_PerfStat>();
			Setting->SetStat(EYanDisplayablePerformanceStat::PacketSize_Outgoing);
			Setting->SetDisplayName(LOCTEXT("PerfStat_PacketSize_Outgoing", "出向包大小"));
			Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_PacketSize_Outgoing", "最近一秒发出的数据包平均字节数。"));
			StatCategory_Network->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
	}

	// 延迟
	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* StatCategory_Latency = NewObject<UGameSettingCollection>();
		StatCategory_Latency->SetDevName(TEXT("StatCategory_Latency"));
		StatCategory_Latency->SetDisplayName(LOCTEXT("StatCategory_Latency_Name", "延迟"));
		StatsPage->AddSetting(StatCategory_Latency);

		//----------------------------------------------------------------------------------
		{
			UGameSettingValueDiscreteDynamic_Bool* Setting = NewObject<UGameSettingValueDiscreteDynamic_Bool>();
			Setting->SetDevName(TEXT("InputLatencyTrackingStats"));
			Setting->SetDisplayName(LOCTEXT("InputLatencyTrackingStats_Name", "开启延迟统计"));
			Setting->SetDescriptionRichText(LOCTEXT("InputLatencyTrackingStats_Description", "采集从输入到画面呈现的端到端延迟数据。"));

			Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetEnableLatencyTrackingStats));
			Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetEnableLatencyTrackingStats));

			Setting->SetDefaultValue(UYanSettingsLocal::DoesPlatformSupportLatencyTrackingStats());
			Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyStatsSupported>());

			StatCategory_Latency->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueDiscreteDynamic_Bool* Setting = NewObject<UGameSettingValueDiscreteDynamic_Bool>();
			Setting->SetDevName(TEXT("InputLatencyMarkers"));
			Setting->SetDisplayName(LOCTEXT("InputLatencyMarkers_Name", "开启延迟标记"));
			Setting->SetDescriptionRichText(LOCTEXT("InputLatencyMarkers_Description", "开火时闪烁屏幕，便于用外部设备测量输入延迟。"));

			Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetEnableLatencyFlashIndicators));
			Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetEnableLatencyFlashIndicators));
			Setting->SetDefaultValue(false);

			Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyStatsSupported>());
			Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyStatsCurrentlyEnabled>());
			Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyMarkersSupported>());

			StatCategory_Latency->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_PerfStat* Setting = NewObject<UYanSettingValueDiscrete_PerfStat>();
			Setting->SetStat(EYanDisplayablePerformanceStat::Latency_Total);
			Setting->SetDisplayName(LOCTEXT("PerfStat_Latency_Total", "总延迟"));
			Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_Latency_Total", "从输入到画面呈现的总延迟。"));
			Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyStatsSupported>());
			Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyStatsCurrentlyEnabled>());
			StatCategory_Latency->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_PerfStat* Setting = NewObject<UYanSettingValueDiscrete_PerfStat>();
			Setting->SetStat(EYanDisplayablePerformanceStat::Latency_Game);
			Setting->SetDisplayName(LOCTEXT("PerfStat_Latency_Game", "游戏延迟"));
			Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_Latency_Game", "游戏模拟开始到驱动提交结束的耗时。"));
			Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyStatsSupported>());
			Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyStatsCurrentlyEnabled>());
			StatCategory_Latency->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_PerfStat* Setting = NewObject<UYanSettingValueDiscrete_PerfStat>();
			Setting->SetStat(EYanDisplayablePerformanceStat::Latency_Render);
			Setting->SetDisplayName(LOCTEXT("PerfStat_Latency_Render", "渲染延迟"));
			Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_Latency_Render", "系统渲染队列开始到 GPU 渲染结束的耗时。"));
			Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyStatsSupported>());
			Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyStatsCurrentlyEnabled>());
			StatCategory_Latency->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
	}

	// 移速
	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* StatCategory_MovementSpeed = NewObject<UGameSettingCollection>();
		StatCategory_MovementSpeed->SetDevName(TEXT("StatCategory_MovementSpeed"));
		StatCategory_MovementSpeed->SetDisplayName(LOCTEXT("StatCategory_MovementSpeed_Name", "移速"));
		StatsPage->AddSetting(StatCategory_MovementSpeed);

		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_PerfStat* Setting = NewObject<UYanSettingValueDiscrete_PerfStat>();
			Setting->SetStat(EYanDisplayablePerformanceStat::MovementSpeed);
			Setting->SetDisplayName(LOCTEXT("PerfStat_MovementSpeed", "移动速度"));
			Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_MovementSpeed", "角色当前的三维移动速度（cm/s），空中与地面均计入。"));
			StatCategory_MovementSpeed->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
	}
}

#undef LOCTEXT_NAMESPACE
