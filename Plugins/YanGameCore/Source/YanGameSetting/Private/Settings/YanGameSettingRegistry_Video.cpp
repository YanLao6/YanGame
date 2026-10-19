// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditCondition/WhenCondition.h"
#include "EditCondition/WhenPlayingAsPrimaryPlayer.h"
#include "GameSettingAction.h"
#include "GameSettingCollection.h"
#include "GameSettingValueDiscreteDynamic.h"
#include "GameSettingValueScalarDynamic.h"
#include "RHIGlobals.h"
#include "Settings/CustomSettings/YanSettingValueDiscrete_OverallQuality.h"
#include "Settings/CustomSettings/YanSettingValueDiscrete_Resolution.h"
#include "Settings/YanGameSettingRegistry.h"
#include "Settings/YanSettingsLocal.h"

#define LOCTEXT_NAMESPACE "Yan"

// 可选的帧率上限档位。数值需要与常见显示器刷新率对应，超出上限的项对玩家没有意义
static const TArray<int32> DesktopFrameRateLimits = { 30, 60, 90, 120, 144, 165, 240 };

//////////////////////////////////////////////////////////////////////

// 细分画质项改动后立即生效，玩家无需点击应用即可看到画面变化
class FYanSettingEditCondition_ApplyScalabilityOnChange : public FGameSettingEditCondition
{
public:
	virtual void SettingChanged(const ULocalPlayer* LocalPlayer, UGameSetting* Setting, EGameSettingChangeReason Reason) const override
	{
		UYanSettingsLocal::Get()->ApplyScalabilitySettings();
	}
};

// 动态分辨率依赖 RHI 支持，不支持时该项没有意义
class FYanSettingEditCondition_DynamicResolution : public FGameSettingEditCondition
{
public:
	virtual void GatherEditState(const ULocalPlayer* InLocalPlayer, FGameSettingEditableState& InOutEditState) const override
	{
		if (!GRHISupportsDynamicResolution)
		{
			InOutEditState.Kill(TEXT("当前 RHI 不支持动态分辨率"));
		}
	}
};

//////////////////////////////////////////////////////////////////////

static void AddFrameRateOptions(UGameSettingValueDiscreteDynamic_Number* Setting)
{
	const FText FPSFormat = LOCTEXT("FPSFormat", "{0} FPS");
	for (int32 Rate : DesktopFrameRateLimits)
	{
		Setting->AddOption((float)Rate, FText::Format(FPSFormat, Rate));
	}

	Setting->AddOption(0.0f, LOCTEXT("UnlimitedFPS", "不限制"));
}

static void AddDynamicResolutionOptions(UGameSettingValueDiscreteDynamic_Number* Setting)
{
	const FText FPSFormat = LOCTEXT("FPSFormat", "{0} FPS");
	for (int32 Rate : DesktopFrameRateLimits)
	{
		Setting->AddOption((float)Rate, FText::Format(FPSFormat, Rate));
	}

	Setting->AddOption(0.0f, LOCTEXT("DynamicResolutionOff", "关闭"));
}

//////////////////////////////////////////////////////////////////////

UGameSettingCollection* UYanGameSettingRegistry::InitializeVideoSettings(ULocalPlayer* InLocalPlayer)
{
	UGameSettingCollection* Screen = NewObject<UGameSettingCollection>();
	Screen->SetDevName(TEXT("VideoCollection"));
	Screen->SetDisplayName(LOCTEXT("VideoCollection_Name", "画面"));
	Screen->Initialize(InLocalPlayer);

	UGameSettingValueDiscreteDynamic_Enum* WindowModeSetting = nullptr;

	// 显示
	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* Display = NewObject<UGameSettingCollection>();
		Display->SetDevName(TEXT("DisplayCollection"));
		Display->SetDisplayName(LOCTEXT("DisplayCollection_Name", "显示"));
		Screen->AddSetting(Display);

		//----------------------------------------------------------------------------------
		{
			UGameSettingValueDiscreteDynamic_Enum* Setting = NewObject<UGameSettingValueDiscreteDynamic_Enum>();
			Setting->SetDevName(TEXT("WindowMode"));
			Setting->SetDisplayName(LOCTEXT("WindowMode_Name", "窗口模式"));
			Setting->SetDescriptionRichText(LOCTEXT("WindowMode_Description", "窗口模式下可以自由调整窗口大小并与其他程序交互；无边框窗口便于快速切换程序；全屏模式下切换程序不便，但运行效率略高。"));

			Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetFullscreenMode));
			Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetFullscreenMode));
			Setting->AddEnumOption(EWindowMode::Fullscreen, LOCTEXT("WindowModeFullscreen", "全屏"));
			Setting->AddEnumOption(EWindowMode::WindowedFullscreen, LOCTEXT("WindowModeWindowedFullscreen", "无边框窗口"));
			Setting->AddEnumOption(EWindowMode::Windowed, LOCTEXT("WindowModeWindowed", "窗口"));

			WindowModeSetting = Setting;

			Display->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_Resolution* Setting = NewObject<UYanSettingValueDiscrete_Resolution>();
			Setting->SetDevName(TEXT("Resolution"));
			Setting->SetDisplayName(LOCTEXT("Resolution_Name", "分辨率"));
			Setting->SetDescriptionRichText(LOCTEXT("Resolution_Description", "窗口模式下决定窗口大小；全屏模式下决定显卡输出分辨率，与显示器不匹配时可能出现黑边；无边框窗口模式下不可调整。"));

			Setting->AddEditDependency(WindowModeSetting);
			Setting->AddEditCondition(MakeShared<FWhenCondition>([WindowModeSetting](const ULocalPlayer*, FGameSettingEditableState& InOutEditState)
				{
					if (WindowModeSetting->GetValue<EWindowMode::Type>() == EWindowMode::WindowedFullscreen)
					{
						InOutEditState.Disable(LOCTEXT("ResolutionWindowedFullscreen_Disabled", "窗口模式为<strong>无边框窗口</>时，分辨率必须与桌面分辨率一致。"));
					}
				}));

			Display->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			AddPerformanceStatPage(Display, InLocalPlayer);
		}
	}

	// 图像
	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* Graphics = NewObject<UGameSettingCollection>();
		Graphics->SetDevName(TEXT("GraphicsCollection"));
		Graphics->SetDisplayName(LOCTEXT("GraphicsCollection_Name", "图像"));
		Screen->AddSetting(Graphics);

		//----------------------------------------------------------------------------------
		{
			UGameSettingValueScalarDynamic* Setting = NewObject<UGameSettingValueScalarDynamic>();
			Setting->SetDevName(TEXT("Brightness"));
			Setting->SetDisplayName(LOCTEXT("Brightness_Name", "亮度"));
			Setting->SetDescriptionRichText(LOCTEXT("Brightness_Description", "调整画面整体亮度。"));

			Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetDisplayGamma));
			Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetDisplayGamma));
			Setting->SetDefaultValue(2.2);
			Setting->SetDisplayFormat([](double SourceValue, double NormalizedValue)
				{
					return FText::Format(LOCTEXT("BrightnessFormat", "{0}%"), (int32)FMath::GetMappedRangeValueClamped(FVector2D(0, 1), FVector2D(50, 150), NormalizedValue));
				});
			Setting->SetSourceRangeAndStep(TRange<double>(1.7, 2.7), 0.01);

			// Gamma 是整个渲染输出的全局状态，分屏时只允许主玩家调整
			Setting->AddEditCondition(FWhenPlayingAsPrimaryPlayer::Get());

			Graphics->AddSetting(Setting);
		}
	}

	// 画质
	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* GraphicsQuality = NewObject<UGameSettingCollection>();
		GraphicsQuality->SetDevName(TEXT("GraphicsQuality"));
		GraphicsQuality->SetDisplayName(LOCTEXT("GraphicsQuality_Name", "画质"));
		Screen->AddSetting(GraphicsQuality);

		UGameSettingAction* AutoSetQuality = nullptr;
		UYanSettingValueDiscrete_OverallQuality* GraphicsQualityPresets = nullptr;

		//----------------------------------------------------------------------------------
		{
			UGameSettingAction* Setting = NewObject<UGameSettingAction>();
			Setting->SetDevName(TEXT("AutoSetQuality"));
			Setting->SetDisplayName(LOCTEXT("AutoSetQuality_Name", "自动检测画质"));
			Setting->SetDescriptionRichText(LOCTEXT("AutoSetQuality_Description", "运行硬件基准测试，并据此自动配置各项画质选项。"));

			Setting->SetDoesActionDirtySettings(true);
			Setting->SetActionText(LOCTEXT("AutoSetQuality_Action", "自动检测"));
			Setting->SetCustomAction([](ULocalPlayer* LocalPlayer)
				{
					// 检测结果要等玩家点击应用后才落盘
					constexpr bool bImmediatelySaveState = false;
					UYanSettingsLocal::Get()->RunAutoBenchmark(bImmediatelySaveState);
				});

			Setting->AddEditCondition(MakeShared<FWhenCondition>([](const ULocalPlayer* LocalPlayer, FGameSettingEditableState& InOutEditState)
				{
					if (!UYanSettingsLocal::Get()->CanRunAutoBenchmark())
					{
						InOutEditState.Kill(TEXT("当前平台不支持自动画质检测"));
					}
				}));

			GraphicsQuality->AddSetting(Setting);

			AutoSetQuality = Setting;
		}
		//----------------------------------------------------------------------------------
		{
			UYanSettingValueDiscrete_OverallQuality* Setting = NewObject<UYanSettingValueDiscrete_OverallQuality>();
			Setting->SetDevName(TEXT("GraphicsQualityPresets"));
			Setting->SetDisplayName(LOCTEXT("GraphicsQualityPresets_Name", "画质预设"));
			Setting->SetDescriptionRichText(LOCTEXT("GraphicsQualityPresets_Description", "画质预设可一次性调整多个画面选项。可以先试用几档，找到画面与性能的平衡点。"));

			Setting->AddEditDependency(AutoSetQuality);

			GraphicsQuality->AddSetting(Setting);

			GraphicsQualityPresets = Setting;
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueScalarDynamic* Setting = NewObject<UGameSettingValueScalarDynamic>();
			Setting->SetDevName(TEXT("ResolutionScale"));
			Setting->SetDisplayName(LOCTEXT("ResolutionScale_Name", "3D 分辨率"));
			Setting->SetDescriptionRichText(LOCTEXT("ResolutionScale_Description", "3D 分辨率决定游戏内物体的渲染分辨率，不影响菜单界面。降低该值可显著提升帧率。"));

			Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetResolutionScaleNormalized));
			Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetResolutionScaleNormalized));
			Setting->SetDisplayFormat(UGameSettingValueScalarDynamic::ZeroToOnePercent);

			Setting->AddEditDependency(AutoSetQuality);
			Setting->AddEditDependency(GraphicsQualityPresets);
			Setting->AddEditCondition(MakeShared<FYanSettingEditCondition_ApplyScalabilityOnChange>());

			// 单项改动会把预设切换为"自定义"，因此预设项也要依赖本项
			GraphicsQualityPresets->AddEditDependency(Setting);
			GraphicsQuality->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------

		// 各细分画质项结构一致，统一按四档构建
		auto AddQualitySetting = [&](const FName& DevName, const FText& DisplayName, const FText& Description,
			const TSharedRef<FGameSettingDataSource>& Getter, const TSharedRef<FGameSettingDataSource>& Setter,
			const TArray<FText>& OptionNames)
			{
				UGameSettingValueDiscreteDynamic_Number* Setting = NewObject<UGameSettingValueDiscreteDynamic_Number>();
				Setting->SetDevName(DevName);
				Setting->SetDisplayName(DisplayName);
				Setting->SetDescriptionRichText(Description);

				Setting->SetDynamicGetter(Getter);
				Setting->SetDynamicSetter(Setter);

				for (int32 Index = 0; Index < OptionNames.Num(); ++Index)
				{
					Setting->AddOption(Index, OptionNames[Index]);
				}

				Setting->AddEditDependency(AutoSetQuality);
				Setting->AddEditDependency(GraphicsQualityPresets);
				Setting->AddEditCondition(MakeShared<FYanSettingEditCondition_ApplyScalabilityOnChange>());

				GraphicsQualityPresets->AddEditDependency(Setting);
				GraphicsQuality->AddSetting(Setting);
			};

		const TArray<FText> LowToEpic = {
			LOCTEXT("QualityLow", "低"),
			LOCTEXT("QualityMedium", "中"),
			LOCTEXT("QualityHigh", "高"),
			LOCTEXT("QualityEpic", "极高")
		};

		AddQualitySetting(TEXT("GlobalIlluminationQuality"),
			LOCTEXT("GlobalIlluminationQuality_Name", "全局光照"),
			LOCTEXT("GlobalIlluminationQuality_Description", "控制动态间接光、天空遮蔽与环境光遮蔽的质量。高及以上档位使用更精确的光线追踪方案，性能开销更大。"),
			GET_LOCAL_SETTINGS_FUNCTION_PATH(GetGlobalIlluminationQuality),
			GET_LOCAL_SETTINGS_FUNCTION_PATH(SetGlobalIlluminationQuality),
			LowToEpic);

		AddQualitySetting(TEXT("Shadows"),
			LOCTEXT("Shadows_Name", "阴影"),
			LOCTEXT("Shadows_Description", "控制阴影的分辨率与可见距离。"),
			GET_LOCAL_SETTINGS_FUNCTION_PATH(GetShadowQuality),
			GET_LOCAL_SETTINGS_FUNCTION_PATH(SetShadowQuality),
			{ LOCTEXT("ShadowOff", "关闭"), LowToEpic[1], LowToEpic[2], LowToEpic[3] });

		AddQualitySetting(TEXT("AntiAliasing"),
			LOCTEXT("AntiAliasing_Name", "抗锯齿"),
			LOCTEXT("AntiAliasing_Description", "控制物体边缘的平滑程度。"),
			GET_LOCAL_SETTINGS_FUNCTION_PATH(GetAntiAliasingQuality),
			GET_LOCAL_SETTINGS_FUNCTION_PATH(SetAntiAliasingQuality),
			{ LOCTEXT("AntiAliasingOff", "关闭"), LowToEpic[1], LowToEpic[2], LowToEpic[3] });

		AddQualitySetting(TEXT("ViewDistance"),
			LOCTEXT("ViewDistance_Name", "可视距离"),
			LOCTEXT("ViewDistance_Description", "控制远处物体的显示距离。"),
			GET_LOCAL_SETTINGS_FUNCTION_PATH(GetViewDistanceQuality),
			GET_LOCAL_SETTINGS_FUNCTION_PATH(SetViewDistanceQuality),
			{ LOCTEXT("ViewDistanceNear", "近"), LOCTEXT("ViewDistanceMedium", "中"), LOCTEXT("ViewDistanceFar", "远"), LowToEpic[3] });

		AddQualitySetting(TEXT("TextureQuality"),
			LOCTEXT("TextureQuality_Name", "纹理质量"),
			LOCTEXT("TextureQuality_Description", "控制纹理分辨率。显存不足时降低该项可减少卡顿。"),
			GET_LOCAL_SETTINGS_FUNCTION_PATH(GetTextureQuality),
			GET_LOCAL_SETTINGS_FUNCTION_PATH(SetTextureQuality),
			LowToEpic);

		AddQualitySetting(TEXT("VisualEffectQuality"),
			LOCTEXT("VisualEffectQuality_Name", "特效质量"),
			LOCTEXT("VisualEffectQuality_Description", "控制粒子等视觉特效的质量与数量。"),
			GET_LOCAL_SETTINGS_FUNCTION_PATH(GetVisualEffectQuality),
			GET_LOCAL_SETTINGS_FUNCTION_PATH(SetVisualEffectQuality),
			LowToEpic);

		AddQualitySetting(TEXT("ReflectionQuality"),
			LOCTEXT("ReflectionQuality_Name", "反射质量"),
			LOCTEXT("ReflectionQuality_Description", "控制反射效果的精度。"),
			GET_LOCAL_SETTINGS_FUNCTION_PATH(GetReflectionQuality),
			GET_LOCAL_SETTINGS_FUNCTION_PATH(SetReflectionQuality),
			LowToEpic);

		AddQualitySetting(TEXT("PostProcessingQuality"),
			LOCTEXT("PostProcessingQuality_Name", "后处理质量"),
			LOCTEXT("PostProcessingQuality_Description", "控制景深、运动模糊等后期效果的质量。"),
			GET_LOCAL_SETTINGS_FUNCTION_PATH(GetPostProcessingQuality),
			GET_LOCAL_SETTINGS_FUNCTION_PATH(SetPostProcessingQuality),
			LowToEpic);
	}

	// 高级
	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* AdvancedGraphics = NewObject<UGameSettingCollection>();
		AdvancedGraphics->SetDevName(TEXT("AdvancedGraphics"));
		AdvancedGraphics->SetDisplayName(LOCTEXT("AdvancedGraphics_Name", "高级"));
		Screen->AddSetting(AdvancedGraphics);

		//----------------------------------------------------------------------------------
		{
			UGameSettingValueDiscreteDynamic_Bool* Setting = NewObject<UGameSettingValueDiscreteDynamic_Bool>();
			Setting->SetDevName(TEXT("VerticalSync"));
			Setting->SetDisplayName(LOCTEXT("VerticalSync_Name", "垂直同步"));
			Setting->SetDescriptionRichText(LOCTEXT("VerticalSync_Description", "开启垂直同步可消除画面撕裂；关闭后帧率更高、输入响应更快，但可能出现横向撕裂。"));

			Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(IsVSyncEnabled));
			Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetVSyncEnabled));
			Setting->SetDefaultValue(false);

			Setting->AddEditDependency(WindowModeSetting);
			Setting->AddEditCondition(MakeShared<FWhenCondition>([WindowModeSetting](const ULocalPlayer*, FGameSettingEditableState& InOutEditState)
				{
					if (WindowModeSetting->GetValue<EWindowMode::Type>() != EWindowMode::Fullscreen)
					{
						InOutEditState.Disable(LOCTEXT("FullscreenNeededForVSync", "该功能仅在窗口模式为“全屏”时可用。"));
					}
				}));

			AdvancedGraphics->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueDiscreteDynamic_Enum* Setting = NewObject<UGameSettingValueDiscreteDynamic_Enum>();
			Setting->SetDevName(TEXT("AntiAliasingMethod"));
			Setting->SetDisplayName(LOCTEXT("AntiAliasingMethod_Name", "抗锯齿方法"));
			Setting->SetDescriptionRichText(LOCTEXT("AntiAliasingMethod_Description", "决定使用哪种抗锯齿算法。TSR 画质最好，在较低渲染分辨率下也能重建出清晰画面，但开销最高；TAA 开销中等；SMAA 与 FXAA 属单帧算法，开销低但边缘略模糊，运动时不产生拖影。上方“抗锯齿”一项控制的是所选算法的质量档位，两者相互独立。"));

			Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetAntiAliasingMethod));
			Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetAntiAliasingMethod));

			// 不提供 MSAA：它仅在前向渲染下有效，本项目使用延迟渲染，选中后不会生效
			Setting->AddEnumOption(AAM_TSR, LOCTEXT("AntiAliasingMethod_TSR", "TSR（时域超分辨率）"));
			Setting->AddEnumOption(AAM_TemporalAA, LOCTEXT("AntiAliasingMethod_TAA", "TAA（时域抗锯齿）"));
			Setting->AddEnumOption(AAM_SMAA, LOCTEXT("AntiAliasingMethod_SMAA", "SMAA（形态学抗锯齿）"));
			Setting->AddEnumOption(AAM_FXAA, LOCTEXT("AntiAliasingMethod_FXAA", "FXAA（快速近似抗锯齿）"));
			Setting->AddEnumOption(AAM_None, LOCTEXT("AntiAliasingMethod_None", "关闭"));

			AdvancedGraphics->AddSetting(Setting);
		}
	}

	return Screen;
}

void UYanGameSettingRegistry::InitializeVideoSettings_FrameRates(UGameSettingCollection* Screen, ULocalPlayer* InLocalPlayer)
{
	//----------------------------------------------------------------------------------
	{
		UGameSettingValueDiscreteDynamic_Number* Setting = NewObject<UGameSettingValueDiscreteDynamic_Number>();
		Setting->SetDevName(TEXT("FrameRateLimit_Always"));
		Setting->SetDisplayName(LOCTEXT("FrameRateLimit_Always_Name", "帧率上限"));
		Setting->SetDescriptionRichText(LOCTEXT("FrameRateLimit_Always_Description", "设定允许的最高帧率。调低可获得更稳定的帧率，调高则在性能足够时体验更好。要达到高帧率可能需要关闭垂直同步。"));

		Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetFrameRateLimit_Always));
		Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetFrameRateLimit_Always));
		Setting->SetDefaultValue(GetDefault<UYanSettingsLocal>()->GetFrameRateLimit_Always());

		AddFrameRateOptions(Setting);

		Screen->AddSetting(Setting);
	}
	//----------------------------------------------------------------------------------
	{
		UGameSettingValueDiscreteDynamic_Number* Setting = NewObject<UGameSettingValueDiscreteDynamic_Number>();
		Setting->SetDevName(TEXT("FrameRateLimit_InMenu"));
		Setting->SetDisplayName(LOCTEXT("FrameRateLimit_InMenu_Name", "帧率上限（菜单）"));
		Setting->SetDescriptionRichText(LOCTEXT("FrameRateLimit_InMenu_Description", "位于菜单界面时的帧率上限。"));

		Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetFrameRateLimit_InMenu));
		Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetFrameRateLimit_InMenu));
		Setting->SetDefaultValue(GetDefault<UYanSettingsLocal>()->GetFrameRateLimit_InMenu());

		AddFrameRateOptions(Setting);

		Screen->AddSetting(Setting);
	}
	//----------------------------------------------------------------------------------
	{
		UGameSettingValueDiscreteDynamic_Number* Setting = NewObject<UGameSettingValueDiscreteDynamic_Number>();
		Setting->SetDevName(TEXT("FrameRateLimit_WhenBackgrounded"));
		Setting->SetDisplayName(LOCTEXT("FrameRateLimit_WhenBackgrounded_Name", "帧率上限（后台）"));
		Setting->SetDescriptionRichText(LOCTEXT("FrameRateLimit_WhenBackgrounded_Description", "窗口失去焦点时的帧率上限。"));

		Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetFrameRateLimit_WhenBackgrounded));
		Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetFrameRateLimit_WhenBackgrounded));
		Setting->SetDefaultValue(GetDefault<UYanSettingsLocal>()->GetFrameRateLimit_WhenBackgrounded());

		AddFrameRateOptions(Setting);

		Screen->AddSetting(Setting);
	}
	//----------------------------------------------------------------------------------
	{
		UGameSettingValueDiscreteDynamic_Number* Setting = NewObject<UGameSettingValueDiscreteDynamic_Number>();
		Setting->SetDevName(TEXT("FrameRateLimit_OnBattery"));
		Setting->SetDisplayName(LOCTEXT("FrameRateLimit_OnBattery_Name", "帧率上限（电池供电）"));
		Setting->SetDescriptionRichText(LOCTEXT("FrameRateLimit_OnBattery_Description", "使用电池供电时的帧率上限，调低可延长续航。"));

		Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetFrameRateLimit_OnBattery));
		Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetFrameRateLimit_OnBattery));
		Setting->SetDefaultValue(GetDefault<UYanSettingsLocal>()->GetFrameRateLimit_OnBattery());

		AddFrameRateOptions(Setting);

		Screen->AddSetting(Setting);
	}
	//----------------------------------------------------------------------------------
	{
		UGameSettingValueDiscreteDynamic_Number* Setting = NewObject<UGameSettingValueDiscreteDynamic_Number>();
		Setting->SetDevName(TEXT("DynamicResolution"));
		Setting->SetDisplayName(LOCTEXT("DynamicResolution_Name", "动态分辨率"));
		Setting->SetDescriptionRichText(LOCTEXT("DynamicResolution_Description", "开启后渲染分辨率会动态缩放，以尽量维持所选的目标帧率。"));

		Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetDynamicResolutionFrameRateTarget));
		Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetDynamicResolutionFrameRateTarget));
		Setting->SetDefaultValue(0.0f);
		Setting->AddEditCondition(MakeShared<FYanSettingEditCondition_DynamicResolution>());

		AddDynamicResolutionOptions(Setting);

		Screen->AddSetting(Setting);
	}
}

#undef LOCTEXT_NAMESPACE
