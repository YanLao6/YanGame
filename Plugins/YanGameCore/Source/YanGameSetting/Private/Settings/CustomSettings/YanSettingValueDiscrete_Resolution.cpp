// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/CustomSettings/YanSettingValueDiscrete_Resolution.h"

#include "DynamicRHI.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "UnrealEngine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanSettingValueDiscrete_Resolution)

#define LOCTEXT_NAMESPACE "YanSettings"

void UYanSettingValueDiscrete_Resolution::BeginDestroy()
{
	Super::BeginDestroy();

	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication();
		if (PlatformApplication.IsValid())
		{
			GenericApplication::FOnDisplayMetricsChanged& DisplayMetricsChangedEvent = PlatformApplication->OnDisplayMetricsChanged();
			DisplayMetricsChangedEvent.Remove(DisplayMetricsChangedHandle);
		}
	}
}

void UYanSettingValueDiscrete_Resolution::OnInitialized()
{
	Super::OnInitialized();

	TSharedPtr<GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication();
	if (ensure(PlatformApplication.IsValid()))
	{
		FDisplayMetrics::RebuildDisplayMetrics(CurrentDisplayMetrics);

		// 显示器热插拔或分辨率变化后需要重建可选列表
		GenericApplication::FOnDisplayMetricsChanged& DisplayMetricsChangedEvent = PlatformApplication->OnDisplayMetricsChanged();
		if (!DisplayMetricsChangedEvent.IsBoundToObject(this))
		{
			DisplayMetricsChangedHandle = DisplayMetricsChangedEvent.AddUObject(this, &UYanSettingValueDiscrete_Resolution::OnDisplayMetricsChanged);
		}
	}

	InitializeResolutions();
}

void UYanSettingValueDiscrete_Resolution::StoreInitial()
{
}

void UYanSettingValueDiscrete_Resolution::ResetToDefault()
{
}

void UYanSettingValueDiscrete_Resolution::RestoreToInitial()
{
}

void UYanSettingValueDiscrete_Resolution::SetDiscreteOptionByIndex(int32 Index)
{
	TArrayView<const TSharedPtr<FScreenResolutionEntry>> Resolutions = GetSelectedResolutionList();
	if (Resolutions.IsValidIndex(Index) && Resolutions[Index].IsValid())
	{
		GEngine->GetGameUserSettings()->SetScreenResolution(Resolutions[Index]->GetResolution());
		NotifySettingChanged(EGameSettingChangeReason::Change);
	}
}

int32 UYanSettingValueDiscrete_Resolution::GetDiscreteOptionIndex() const
{
	const UGameUserSettings* UserSettings = GEngine->GetGameUserSettings();

	return FindIndexOfDisplayResolution(UserSettings->GetScreenResolution());
}

TArray<FText> UYanSettingValueDiscrete_Resolution::GetDiscreteOptions() const
{
	TArray<FText> ReturnResolutionTexts;

	TArrayView<const TSharedPtr<FScreenResolutionEntry>> Resolutions = GetSelectedResolutionList();
	for (int32 i = 0; i < Resolutions.Num(); ++i)
	{
		ReturnResolutionTexts.Add(Resolutions[i]->GetDisplayText());
	}

	return ReturnResolutionTexts;
}

void UYanSettingValueDiscrete_Resolution::OnDependencyChanged()
{
	// 窗口模式改变会切换可选列表，需要把当前分辨率对齐到新列表中最接近的一项
	InitializeResolutions();
	const FIntPoint CurrentResolution = GEngine->GetGameUserSettings()->GetScreenResolution();
	SetDiscreteOptionByIndex(FindClosestResolutionIndex(CurrentResolution));
}

void UYanSettingValueDiscrete_Resolution::OnDisplayMetricsChanged(const FDisplayMetrics& NewDisplayMetrics)
{
	CurrentDisplayMetrics = NewDisplayMetrics;
	InitializeResolutions();
}

const FMonitorInfo* UYanSettingValueDiscrete_Resolution::GetCurrentMonitor() const
{
	const UGameUserSettings* UserSettings = GEngine->GetGameUserSettings();
	const FString DisplayID = UserSettings->GetDisplayID();
	const int32 DisplayIndex = UserSettings->GetDisplayIndex();
	const int32 MonitorIndex = CurrentDisplayMetrics.GetClosestMonitorFromIDAndIndex(DisplayID, DisplayIndex);
	return CurrentDisplayMetrics.MonitorInfo.IsValidIndex(MonitorIndex) ? &CurrentDisplayMetrics.MonitorInfo[MonitorIndex] : nullptr;
}

void UYanSettingValueDiscrete_Resolution::InitializeResolutions()
{
	ResolutionsFullscreen.Empty();
	ResolutionsWindowed.Empty();
	ResolutionsWindowedFullscreen.Empty();

	const FMonitorInfo* const Monitor = GetCurrentMonitor();
	FScreenResolutionArray ResArray;
	if (Monitor)
	{
		RHIGetAvailableResolutionsForDisplay(ResArray, true, Monitor->NativeHandle);
	}
	else
	{
		RHIGetAvailableResolutions(ResArray, true);
	}

	// 窗口模式：取标准分辨率表中不超过工作区大小的项
	{
		TArray<FIntPoint> WindowedResolutions;
		const FIntPoint MinResolution(1280, 720);
		FIntPoint MaxResolution;
		if (Monitor)
		{
			MaxResolution = FIntPoint(Monitor->WorkArea.Right - Monitor->WorkArea.Left, Monitor->WorkArea.Bottom - Monitor->WorkArea.Top);
		}
		else
		{
			MaxResolution = FIntPoint(CurrentDisplayMetrics.PrimaryDisplayWorkAreaRect.Right - CurrentDisplayMetrics.PrimaryDisplayWorkAreaRect.Left,
				CurrentDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - CurrentDisplayMetrics.PrimaryDisplayWorkAreaRect.Top);
		}

		// 排除 4:3 及更窄的比例
		const float MinAspectRatio = 16 / 10.f;

		if (MaxResolution.X >= MinResolution.X && MaxResolution.Y >= MinResolution.Y)
		{
			GetStandardWindowResolutions(MinResolution, MaxResolution, MinAspectRatio, WindowedResolutions);
		}

		if (GSystemResolution.WindowMode == EWindowMode::Windowed)
		{
			if (GSystemResolution.ResX <= MaxResolution.X && GSystemResolution.ResY <= MaxResolution.Y)
			{
				WindowedResolutions.AddUnique(FIntPoint(GSystemResolution.ResX, GSystemResolution.ResY));
			}
			WindowedResolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });
		}

		// 非标准显示设备上可能一个标准分辨率都不匹配，至少保留主显示器尺寸
		if (WindowedResolutions.Num() == 0)
		{
			WindowedResolutions.Add(FIntPoint(CurrentDisplayMetrics.PrimaryDisplayWidth, CurrentDisplayMetrics.PrimaryDisplayHeight));
		}

		ResolutionsWindowed.Empty(WindowedResolutions.Num());
		for (const FIntPoint& Res : WindowedResolutions)
		{
			TSharedRef<FScreenResolutionEntry> Entry = MakeShared<FScreenResolutionEntry>();
			Entry->Width = Res.X;
			Entry->Height = Res.Y;

			ResolutionsWindowed.Add(Entry);
		}
	}

	// 无边框窗口：只能是显示器原生尺寸
	{
		TSharedRef<FScreenResolutionEntry> Entry = MakeShared<FScreenResolutionEntry>();
		if (Monitor)
		{
			const FPlatformRect& DisplayRect = Monitor->DisplayRect;
			Entry->Width = DisplayRect.Right - DisplayRect.Left;
			Entry->Height = DisplayRect.Bottom - DisplayRect.Top;
		}
		else
		{
			Entry->Width = CurrentDisplayMetrics.PrimaryDisplayWidth;
			Entry->Height = CurrentDisplayMetrics.PrimaryDisplayHeight;
		}

		ResolutionsWindowedFullscreen.Add(Entry);
	}

	// 全屏：先按严格条件筛选，一个都不剩时逐步放宽
	if (!ResArray.IsEmpty())
	{
		for (int32 FilterThreshold = 0; FilterThreshold < 3; ++FilterThreshold)
		{
			for (int32 ModeIndex = 0; ModeIndex < ResArray.Num(); ModeIndex++)
			{
				const FScreenResolutionRHI& ScreenRes = ResArray[ModeIndex];

				if (ShouldAllowFullScreenResolution(ScreenRes, FilterThreshold))
				{
					TSharedRef<FScreenResolutionEntry> Entry = MakeShared<FScreenResolutionEntry>();
					Entry->Width = ScreenRes.Width;
					Entry->Height = ScreenRes.Height;
					Entry->RefreshRate = ScreenRes.RefreshRate;

					ResolutionsFullscreen.Add(Entry);
				}
			}

			if (!ResolutionsFullscreen.IsEmpty())
			{
				break;
			}
		}
	}

	if (ResolutionsFullscreen.IsEmpty())
	{
		ResolutionsFullscreen.Emplace(ResolutionsWindowedFullscreen[0]);
	}
}

TArrayView<const TSharedPtr<UYanSettingValueDiscrete_Resolution::FScreenResolutionEntry>> UYanSettingValueDiscrete_Resolution::GetSelectedResolutionList() const
{
	TArrayView<const TSharedPtr<FScreenResolutionEntry>> Result;

	const EWindowMode::Type WindowMode = GEngine->GetGameUserSettings()->GetFullscreenMode();
	switch (WindowMode)
	{
	case EWindowMode::Windowed:
		Result = MakeArrayView(ResolutionsWindowed);
		break;
	case EWindowMode::WindowedFullscreen:
		Result = MakeArrayView(ResolutionsWindowedFullscreen);
		break;
	case EWindowMode::Fullscreen:
		Result = MakeArrayView(ResolutionsFullscreen);
		break;
	}

	return Result;
}

// 过滤掉会给 UI 带来麻烦的非常规分辨率。
// FilterThreshold 取 0/1/2，数值越大条件越宽松，用于保证至少能列出一项
bool UYanSettingValueDiscrete_Resolution::ShouldAllowFullScreenResolution(const FScreenResolutionRHI& SrcScreenRes, int32 FilterThreshold) const
{
	FScreenResolutionRHI ScreenRes = SrcScreenRes;

	// 预期值：4:3=1.333，16:9=1.777，16:10=1.6，多屏拼接 >2
	bool bIsPortrait = ScreenRes.Width < ScreenRes.Height;
	float AspectRatio = (float)ScreenRes.Width / (float)ScreenRes.Height;

	// 竖屏先翻转成横屏，后续判断就不必区分两种情况
	if (bIsPortrait)
	{
		AspectRatio = 1.0f / AspectRatio;
		ScreenRes.Width = SrcScreenRes.Height;
		ScreenRes.Height = SrcScreenRes.Width;
	}

	if (FilterThreshold < 1)
	{
		// 无法获知显示器原生比例的平台上默认接受，避免把选项全部过滤光
		float DisplayAspect = AspectRatio;

		const FMonitorInfo* const Monitor = GetCurrentMonitor();
		if (Monitor)
		{
			DisplayAspect = (float)Monitor->NativeWidth / (float)Monitor->NativeHeight;
		}
		else
		{
			for (int32 MonitorIndex = 0; MonitorIndex < CurrentDisplayMetrics.MonitorInfo.Num(); ++MonitorIndex)
			{
				const FMonitorInfo& MonitorInfo = CurrentDisplayMetrics.MonitorInfo[MonitorIndex];

				if (MonitorInfo.bIsPrimary)
				{
					DisplayAspect = (float)MonitorInfo.NativeWidth / (float)MonitorInfo.NativeHeight;
					break;
				}
			}
		}

		if (FMath::Abs(DisplayAspect - AspectRatio) > KINDA_SMALL_NUMBER)
		{
			return false;
		}
	}

	if (FilterThreshold < 2 && (ScreenRes.Width < 1280 || ScreenRes.Height < 720))
	{
		return false;
	}

	return true;
}

int32 UYanSettingValueDiscrete_Resolution::FindIndexOfDisplayResolution(const FIntPoint& InPoint) const
{
	TArrayView<const TSharedPtr<FScreenResolutionEntry>> Resolutions = GetSelectedResolutionList();
	for (int32 i = 0, Num = Resolutions.Num(); i < Num; ++i)
	{
		if (Resolutions[i]->GetResolution() == InPoint)
		{
			return i;
		}
	}

	if (!Resolutions.IsEmpty())
	{
		return Resolutions.Num() - 1;
	}

	return INDEX_NONE;
}

int32 UYanSettingValueDiscrete_Resolution::FindClosestResolutionIndex(const FIntPoint& Resolution) const
{
	TArrayView<const TSharedPtr<FScreenResolutionEntry>> Resolutions = GetSelectedResolutionList();
	int32 Index = 0;
	int32 LastDiff = Resolution.SizeSquared();

	// 按对角线平方差比较
	for (int32 i = 0, Num = Resolutions.Num(); i < Num; ++i)
	{
		int32 Diff = FMath::Abs(Resolution.SizeSquared() - Resolutions[i]->GetResolution().SizeSquared());
		if (Diff <= LastDiff)
		{
			Index = i;
		}
		LastDiff = Diff;
	}

	return Index;
}

void UYanSettingValueDiscrete_Resolution::GetStandardWindowResolutions(const FIntPoint& MinResolution, const FIntPoint& MaxResolution, float MinAspectRatio, TArray<FIntPoint>& OutResolutions)
{
	static TArray<FIntPoint> StandardResolutions;
	if (StandardResolutions.Num() == 0)
	{
		// 取自 Wikipedia 的标准显示分辨率表
		{
			StandardResolutions.Emplace(1024, 768);   // XGA
			StandardResolutions.Emplace(1366, 768);   // FWXGA
			StandardResolutions.Emplace(1360, 768);
			StandardResolutions.Emplace(1280, 800);
			StandardResolutions.Emplace(1152, 864);   // XGA+
			StandardResolutions.Emplace(1440, 900);   // WXGA+
			StandardResolutions.Emplace(1280, 1024);  // SXGA
			StandardResolutions.Emplace(1400, 1050);  // SXGA+
			StandardResolutions.Emplace(1680, 1050);  // WSXGA+
			StandardResolutions.Emplace(1600, 1200);  // UXGA
			StandardResolutions.Emplace(1920, 1200);  // WUXGA
		}

		{
			StandardResolutions.Emplace(2048, 1152);  // QWXGA
			StandardResolutions.Emplace(2048, 1536);  // QXGA
			StandardResolutions.Emplace(2560, 1600);  // WQXGA
			StandardResolutions.Emplace(2560, 2048);  // QSXGA
			StandardResolutions.Emplace(3200, 2048);  // WQSXGA
			StandardResolutions.Emplace(3200, 2400);  // QUXGA
			StandardResolutions.Emplace(3840, 2400);  // WQUXGA
		}

		{
			StandardResolutions.Emplace(4096, 3072);  // HXGA
			StandardResolutions.Emplace(5120, 3200);  // WHXGA
			StandardResolutions.Emplace(5120, 4096);  // HSXGA
			StandardResolutions.Emplace(6400, 4096);  // WHSXGA
			StandardResolutions.Emplace(6400, 4800);  // HUXGA
			StandardResolutions.Emplace(7680, 4800);  // WHUXGA
		}

		{
			StandardResolutions.Emplace(640, 360);    // nHD
			StandardResolutions.Emplace(960, 540);    // qHD
			StandardResolutions.Emplace(1280, 720);   // HD
			StandardResolutions.Emplace(1920, 1080);  // FHD
			StandardResolutions.Emplace(2560, 1440);  // QHD
			StandardResolutions.Emplace(3200, 1800);  // WQXGA+
			StandardResolutions.Emplace(3840, 2160);  // UHD 4K
			StandardResolutions.Emplace(4096, 2160);  // DCI 4K
			StandardResolutions.Emplace(7680, 4320);  // FUHD
			StandardResolutions.Emplace(5120, 2160);  // UHD 5K
			StandardResolutions.Emplace(5120, 2880);  // UHD+
			StandardResolutions.Emplace(15360, 8640); // QUHD
		}

		StandardResolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return (A.X * A.Y) < (B.X * B.Y); });
	}

	for (const FIntPoint& Resolution : StandardResolutions)
	{
		if (Resolution.X >= MinResolution.X && Resolution.Y >= MinResolution.Y && Resolution.X <= MaxResolution.X && Resolution.Y <= MaxResolution.Y)
		{
			const float AspectRatio = Resolution.X / (float)Resolution.Y;
			if (AspectRatio > MinAspectRatio || FMath::IsNearlyEqual(AspectRatio, MinAspectRatio))
			{
				OutResolutions.Add(Resolution);
			}
		}
	}
}

FText UYanSettingValueDiscrete_Resolution::FScreenResolutionEntry::GetDisplayText() const
{
	if (!OverrideText.IsEmpty())
	{
		return OverrideText;
	}

	FNumberFormattingOptions Options;
	Options.UseGrouping = false;

	FFormatNamedArguments Args;
	Args.Add(TEXT("X"), FText::AsNumber(Width, &Options));
	Args.Add(TEXT("Y"), FText::AsNumber(Height, &Options));

	return FText::Format(LOCTEXT("ResolutionFormat", "{X} x {Y}"), Args);
}

#undef LOCTEXT_NAMESPACE
