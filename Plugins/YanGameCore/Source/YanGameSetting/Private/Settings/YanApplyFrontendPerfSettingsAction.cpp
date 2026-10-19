// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/YanApplyFrontendPerfSettingsAction.h"

#include "Settings/YanSettingsLocal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanApplyFrontendPerfSettingsAction)

int32 UYanApplyFrontendPerfSettingsAction::ApplicationCounter = 0;

void UYanApplyFrontendPerfSettingsAction::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	ApplicationCounter++;
	if (ApplicationCounter == 1)
	{
		UYanSettingsLocal::Get()->SetShouldUseFrontendPerformanceSettings(true);
	}
}

void UYanApplyFrontendPerfSettingsAction::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	ApplicationCounter--;
	check(ApplicationCounter >= 0);

	if (ApplicationCounter == 0)
	{
		UYanSettingsLocal::Get()->SetShouldUseFrontendPerformanceSettings(false);
	}
}
