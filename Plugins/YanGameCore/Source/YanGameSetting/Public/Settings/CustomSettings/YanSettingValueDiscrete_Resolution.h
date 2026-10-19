// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameSettingValueDiscrete.h"

#include "YanSettingValueDiscrete_Resolution.generated.h"

#define UE_API YANGAMESETTING_API

namespace EWindowMode { enum Type : int; }

struct FScreenResolutionRHI;

/**
 * 分辨率选项。可选列表随窗口模式变化：
 * 全屏取 RHI 报告的显示模式，窗口化取标准分辨率表，无边框窗口固定为显示器原生尺寸。
 */
UCLASS(MinimalAPI)
class UYanSettingValueDiscrete_Resolution : public UGameSettingValueDiscrete
{
	GENERATED_BODY()

public:
	//~Begin UObject Interface
	UE_API virtual void BeginDestroy() override;
	//~End UObject Interface

	//~Begin UGameSettingValue Interface
	UE_API virtual void StoreInitial() override;
	UE_API virtual void ResetToDefault() override;
	UE_API virtual void RestoreToInitial() override;
	//~End UGameSettingValue Interface

	//~Begin UGameSettingValueDiscrete Interface
	UE_API virtual void SetDiscreteOptionByIndex(int32 Index) override;
	UE_API virtual int32 GetDiscreteOptionIndex() const override;
	UE_API virtual TArray<FText> GetDiscreteOptions() const override;
	//~End UGameSettingValueDiscrete Interface

protected:
	//~Begin UGameSettingValue Interface
	UE_API virtual void OnInitialized() override;
	UE_API virtual void OnDependencyChanged() override;
	//~End UGameSettingValue Interface

	void OnDisplayMetricsChanged(const FDisplayMetrics& NewDisplayMetrics);

	const FMonitorInfo* GetCurrentMonitor() const;
	void InitializeResolutions();
	bool ShouldAllowFullScreenResolution(const FScreenResolutionRHI& SrcScreenRes, int32 FilterThreshold) const;
	static void GetStandardWindowResolutions(const FIntPoint& MinResolution, const FIntPoint& MaxResolution, float MinAspectRatio, TArray<FIntPoint>& OutResolutions);
	int32 FindIndexOfDisplayResolution(const FIntPoint& InPoint) const;
	int32 FindClosestResolutionIndex(const FIntPoint& Resolution) const;

	struct FScreenResolutionEntry
	{
		uint32 Width = 0;
		uint32 Height = 0;
		uint32 RefreshRate = 0;
		FText OverrideText;

		FIntPoint GetResolution() const { return FIntPoint(Width, Height); }
		FText GetDisplayText() const;
	};

	TArrayView<const TSharedPtr<FScreenResolutionEntry>> GetSelectedResolutionList() const;

	/** 全屏模式下可用的分辨率 */
	TArray<TSharedPtr<FScreenResolutionEntry>> ResolutionsFullscreen;

	/** 无边框窗口模式下可用的分辨率 */
	TArray<TSharedPtr<FScreenResolutionEntry>> ResolutionsWindowedFullscreen;

	/** 窗口模式下可用的分辨率 */
	TArray<TSharedPtr<FScreenResolutionEntry>> ResolutionsWindowed;

	FDisplayMetrics CurrentDisplayMetrics;
	FDelegateHandle DisplayMetricsChangedHandle;
};

#undef UE_API
