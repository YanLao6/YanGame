// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFeatureAction.h"

#include "YanApplyFrontendPerfSettingsAction.generated.h"

#define UE_API YANGAMESETTING_API

/**
 * GameFeature Action：挂在前端 Experience 上，激活期间让 UYanSettingsLocal 启用菜单专属的性能设置（如 FrameRateLimit_InMenu）。
 *
 * GameUserSettings 是进程级全局对象，而多个 PIE 世界会各自激活一次本 Action，
 * 因此用全局计数合并请求：首次激活开启、最后一次反激活关闭。
 */
UCLASS(MinimalAPI, meta = (DisplayName = "Use Frontend Perf Settings"))
class UYanApplyFrontendPerfSettingsAction final : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	//~Begin UGameFeatureAction Interface
	UE_API virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	UE_API virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	//~End UGameFeatureAction Interface

private:
	static int32 ApplicationCounter;
};

#undef UE_API
