#include "Settings/CustomSettings/YanSettingValueDiscrete_Display.h"

#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "UnrealEngine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanSettingValueDiscrete_Display)

#define LOCTEXT_NAMESPACE "YanSettings"

UYanSettingValueDiscrete_Display::UYanSettingValueDiscrete_Display()
{}

void UYanSettingValueDiscrete_Display::BeginDestroy()
{
	Super::BeginDestroy();

	if (FSlateApplication::IsInitialized())
	{
		if (const TSharedPtr<GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication())
		{
			PlatformApplication->OnDisplayMetricsChanged().Remove(DisplayMetricsChangedHandle);
		}
	}
}

void UYanSettingValueDiscrete_Display::OnInitialized()
{
	Super::OnInitialized();

	const TSharedPtr<GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication();
	if (ensure(PlatformApplication.IsValid()))
	{
		FDisplayMetrics::RebuildDisplayMetrics(CurrentDisplayMetrics);

		GenericApplication::FOnDisplayMetricsChanged& DisplayMetricsChangedEvent = PlatformApplication->OnDisplayMetricsChanged();
		if (!DisplayMetricsChangedEvent.IsBoundToObject(this))
		{
			DisplayMetricsChangedHandle = DisplayMetricsChangedEvent.AddUObject(this, &UYanSettingValueDiscrete_Display::OnDisplayMetricsChanged);
		}
	}
}

void UYanSettingValueDiscrete_Display::StoreInitial()
{
	const UGameUserSettings* UserSettings = GEngine->GetGameUserSettings();

	InitialMonitorID    = UserSettings->GetDisplayID();
	InitialMonitorIndex = UserSettings->GetDisplayIndex();
}

void UYanSettingValueDiscrete_Display::ResetToDefault()
{
	// 无「默认显示器」概念，保持当前选择
}

void UYanSettingValueDiscrete_Display::RestoreToInitial()
{
	// 切换显示器会重建窗口，不做自动回滚
}

void UYanSettingValueDiscrete_Display::SetDiscreteOptionByIndex(int32 Index)
{
	if (CurrentDisplayMetrics.MonitorInfo.IsValidIndex(Index))
	{
		GEngine->GetGameUserSettings()->SetDisplayProperties(CurrentDisplayMetrics.MonitorInfo[Index].ID, Index);
		NotifySettingChanged(EGameSettingChangeReason::Change);
	}
}

int32 UYanSettingValueDiscrete_Display::GetDiscreteOptionIndex() const
{
	const UGameUserSettings* UserSettings = GEngine->GetGameUserSettings();

	return CurrentDisplayMetrics.GetClosestMonitorFromIDAndIndex(UserSettings->GetDisplayID(), UserSettings->GetDisplayIndex());
}

int32 UYanSettingValueDiscrete_Display::GetDiscreteOptionDefaultIndex() const
{
	return CurrentDisplayMetrics.GetClosestMonitorFromIDAndIndex(InitialMonitorID, InitialMonitorIndex);
}

TArray<FText> UYanSettingValueDiscrete_Display::GetDiscreteOptions() const
{
	TArray<FText> Options;

	static const FText UnknownDisplayText = LOCTEXT("UnknownDisplay", "[未知显示器]");

	if (CurrentDisplayMetrics.MonitorInfo.IsEmpty())
	{
		Options.Emplace(UnknownDisplayText);
	}
	else
	{
		for (const FMonitorInfo& Monitor : CurrentDisplayMetrics.MonitorInfo)
		{
			Options.Emplace(Monitor.FriendlyName.IsEmpty()
				                ? (Monitor.Name.IsEmpty() ? UnknownDisplayText : FText::FromString(Monitor.Name))
				                : FText::FromString(Monitor.FriendlyName));
		}
	}

	return Options;
}

void UYanSettingValueDiscrete_Display::OnDependencyChanged()
{
	const UGameUserSettings* UserSettings = GEngine->GetGameUserSettings();

	SetDiscreteOptionByIndex(CurrentDisplayMetrics.GetClosestMonitorFromIDAndIndex(UserSettings->GetDisplayID(), UserSettings->GetDisplayIndex()));
}

void UYanSettingValueDiscrete_Display::OnDisplayMetricsChanged(const FDisplayMetrics& NewDisplayMetrics)
{
	CurrentDisplayMetrics = NewDisplayMetrics;
}

#undef LOCTEXT_NAMESPACE
