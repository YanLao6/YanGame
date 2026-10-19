// 开发期设置与查询实现。

#include "System/YanDeveloperSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanDeveloperSettings)

UYanDeveloperSettings::UYanDeveloperSettings()
{
}

bool UYanDevelopmentStatics::ShouldSkipDirectlyToGameplay()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		return !GetDefault<UYanDeveloperSettings>()->bTestFullGameFlowInPIE;
	}
#endif
	// 打包后一律走完整流程
	return false;
}

bool UYanDevelopmentStatics::ShouldLoadCosmeticBackgrounds()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		return !GetDefault<UYanDeveloperSettings>()->bSkipLoadingCosmeticBackgroundsInPIE;
	}
#endif
	return true;
}

bool UYanDevelopmentStatics::CanPlayerBotsAttack()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		return GetDefault<UYanDeveloperSettings>()->bAllowPlayerBotsToAttack;
	}
#endif
	return true;
}
