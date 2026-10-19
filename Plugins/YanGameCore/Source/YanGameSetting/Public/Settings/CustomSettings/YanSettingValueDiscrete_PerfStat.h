#pragma once

#include "GameSettingValueDiscrete.h"

#include "YanSettingValueDiscrete_PerfStat.generated.h"

#define UE_API YANGAMESETTING_API

enum class EYanDisplayablePerformanceStat : uint8;
enum class EYanStatDisplayMode : uint8;

/**
 * 单项性能统计的显示方式选择项。
 * 一个实例只负责一项统计，由设置树在构建时通过 SetStat 指定。
 */
UCLASS(MinimalAPI)
class UYanSettingValueDiscrete_PerfStat : public UGameSettingValueDiscrete
{
	GENERATED_BODY()

public:
	UE_API UYanSettingValueDiscrete_PerfStat();

	/** 指定本项负责的统计，同时据此生成 DevName 并挂上可见性条件 */
	UE_API void SetStat(EYanDisplayablePerformanceStat InStat);

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

	void AddMode(FText&& Label, EYanStatDisplayMode Mode);

	TArray<FText>               Options;
	TArray<EYanStatDisplayMode> DisplayModes;

	EYanDisplayablePerformanceStat StatToDisplay;
	EYanStatDisplayMode            InitialMode;
};

#undef UE_API
