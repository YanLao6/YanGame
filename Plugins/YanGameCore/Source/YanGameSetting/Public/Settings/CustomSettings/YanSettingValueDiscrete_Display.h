#pragma once

#include "GameSettingValueDiscrete.h"

#include "YanSettingValueDiscrete_Display.generated.h"

#define UE_API YANGAMESETTING_API

/**
 * 显示器选择项。
 * 选项直接来自平台的显示器枚举，会随显示器热插拔更新。
 */
UCLASS(MinimalAPI)
class UYanSettingValueDiscrete_Display : public UGameSettingValueDiscrete
{
	GENERATED_BODY()

public:
	UE_API UYanSettingValueDiscrete_Display();

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
	UE_API virtual int32 GetDiscreteOptionDefaultIndex() const override;
	UE_API virtual TArray<FText> GetDiscreteOptions() const override;
	//~End UGameSettingValueDiscrete Interface

protected:
	//~Begin UGameSettingValue Interface
	UE_API virtual void OnInitialized() override;
	UE_API virtual void OnDependencyChanged() override;
	//~End UGameSettingValue Interface

	void OnDisplayMetricsChanged(const FDisplayMetrics& NewDisplayMetrics);

	FString InitialMonitorID;
	int32   InitialMonitorIndex = -1;

	FDisplayMetrics CurrentDisplayMetrics;
	FDelegateHandle DisplayMetricsChangedHandle;
};

#undef UE_API
