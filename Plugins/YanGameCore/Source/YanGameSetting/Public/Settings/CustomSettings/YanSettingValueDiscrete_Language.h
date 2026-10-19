#pragma once

#include "GameSettingValueDiscrete.h"

#include "YanSettingValueDiscrete_Language.generated.h"

#define UE_API YANGAMESETTING_API

/**
 * 语言选择项。
 * 语言切换需重启才能完全生效，故此处只写入待应用的语言，由共享设置在应用阶段落实。
 */
UCLASS(MinimalAPI)
class UYanSettingValueDiscrete_Language : public UGameSettingValueDiscrete
{
	GENERATED_BODY()

public:
	UE_API UYanSettingValueDiscrete_Language();

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
	UE_API virtual void OnApply() override;
	//~End UGameSettingValue Interface

	// 可选语言列表，索引 0 固定为空串表示跟随系统默认
	TArray<FString> AvailableCultureNames;
};

#undef UE_API
