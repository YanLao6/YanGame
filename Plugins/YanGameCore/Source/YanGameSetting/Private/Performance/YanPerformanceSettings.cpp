#include "Performance/YanPerformanceSettings.h"

#include "Engine/PlatformSettingsManager.h"
#include "Misc/EnumRange.h"
#include "Performance/YanPerformanceStatTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanPerformanceSettings)

UYanPlatformSpecificRenderingSettings::UYanPlatformSpecificRenderingSettings()
{
	MobileFrameRateLimits.Append({20, 30, 45, 60, 90, 120});
}

const UYanPlatformSpecificRenderingSettings* UYanPlatformSpecificRenderingSettings::Get()
{
	UYanPlatformSpecificRenderingSettings* Result = UPlatformSettingsManager::Get().GetSettingsForPlatform<ThisClass>();
	check(Result);

	return Result;
}

UYanPerformanceSettings::UYanPerformanceSettings()
{
	PerPlatformSettings.Initialize(UYanPlatformSpecificRenderingSettings::StaticClass());

	CategoryName = TEXT("Game");

	DesktopFrameRateLimits.Append({30, 60, 120, 144, 160, 165, 180, 200, 240, 360});

	// 默认放开全部统计项
	FYanPerformanceStatGroup& StatGroup = UserFacingPerformanceStats.AddDefaulted_GetRef();
	for (EYanDisplayablePerformanceStat PerfStat : TEnumRange<EYanDisplayablePerformanceStat>())
	{
		StatGroup.AllowedStats.Add(PerfStat);
	}
}
