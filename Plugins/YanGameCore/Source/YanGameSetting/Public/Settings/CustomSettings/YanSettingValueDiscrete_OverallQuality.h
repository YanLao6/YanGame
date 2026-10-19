// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameSettingValueDiscrete.h"

#include "YanSettingValueDiscrete_OverallQuality.generated.h"

#define UE_API YANGAMESETTING_API

/**
 * 画质总档位。各细分画质项被单独调整后，本项会额外显示"自定义"档。
 */
UCLASS(MinimalAPI)
class UYanSettingValueDiscrete_OverallQuality : public UGameSettingValueDiscrete
{
	GENERATED_BODY()

public:
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
	//~End UGameSettingValue Interface

	int32 GetCustomOptionIndex() const;
	int32 GetOverallQualityLevel() const;

	TArray<FText> Options;
	TArray<FText> OptionsWithCustom;
};

#undef UE_API
