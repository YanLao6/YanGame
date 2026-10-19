// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameSettingValueScalar.h"

#include "GameSettingValueScalarDynamic.generated.h"

#define UE_API GAMESETTINGS_API

struct FNumberFormattingOptions;

class FGameSettingDataSource;
class UObject;

//////////////////////////////////////////////////////////////////////////
// UGameSettingValueScalarDynamic
//////////////////////////////////////////////////////////////////////////

typedef TFunction<FText(double SourceValue, double NormalizedValue)> FSettingScalarFormatFunction;

UCLASS(MinimalAPI)
class UGameSettingValueScalarDynamic : public UGameSettingValueScalar
{
	GENERATED_BODY()

public:
	static UE_API FSettingScalarFormatFunction Raw;
	static UE_API FSettingScalarFormatFunction RawOneDecimal;
	static UE_API FSettingScalarFormatFunction RawTwoDecimals;
	static UE_API FSettingScalarFormatFunction ZeroToOnePercent;
	static UE_API FSettingScalarFormatFunction ZeroToOnePercent_OneDecimal;
	static UE_API FSettingScalarFormatFunction SourceAsPercent1;
	static UE_API FSettingScalarFormatFunction SourceAsPercent100;
	static UE_API FSettingScalarFormatFunction SourceAsInteger;
private:
	static const FNumberFormattingOptions& GetOneDecimalFormattingOptions();
	
public:
	UE_API UGameSettingValueScalarDynamic();

	/** UGameSettingValue */
	UE_API virtual void Startup() override;
	UE_API virtual void StoreInitial() override;
	UE_API virtual void ResetToDefault() override;
	UE_API virtual void RestoreToInitial() override;

	/** UGameSettingValueScalar */
	UE_API virtual TOptional<double> GetDefaultValue() const override;
	UE_API virtual void SetValue(double Value, EGameSettingChangeReason Reason = EGameSettingChangeReason::Change) override;
	UE_API virtual double GetValue() const override;
	UE_API virtual TRange<double> GetSourceRange() const override;
	UE_API virtual double GetSourceStep() const override;
	UE_API virtual FText GetFormattedText() const override;

	/** UGameSettingValueDiscreteDynamic */
	UE_API void SetDynamicGetter(const TSharedRef<FGameSettingDataSource>& InGetter);
	UE_API void SetDynamicSetter(const TSharedRef<FGameSettingDataSource>& InSetter);
	UE_API void SetDefaultValue(double InValue);

	/**  */
	UE_API void SetDisplayFormat(FSettingScalarFormatFunction InDisplayFormat);
	
	/**  */
	UE_API void SetSourceRangeAndStep(const TRange<double>& InRange, double InSourceStep);
	
	/**
	 * SetSourceRangeAndStep 定义数值实际可移动的范围，但用户可用的真实下限往往高于该范围的最小值。
	 * 例如某滑条的范围是 0..100，希望它显示为一条从 0 到 100 的完整进度条，
	 * 却不允许用户设置到低于某个下限（如 1）。该下限即 Minimum Limit。
	 */
	UE_API void SetMinimumLimit(const TOptional<double>& InMinimum);

	/**
	 * SetSourceRangeAndStep 定义数值实际可移动的范围，少数情况下用户可用的真实上限低于该范围的最大值。
	 * 例如某滑条的范围是 0..100，希望它显示为一条从 0 到 100 的完整进度条，
	 * 却不允许用户设置到高于某个上限（如 95）。该上限即 Maximum Limit。
	 */
	UE_API void SetMaximumLimit(const TOptional<double>& InMaximum);
	
protected:
	/** UGameSettingValue */
	UE_API virtual void OnInitialized() override;

	UE_API void OnDataSourcesReady();

protected:

	TSharedPtr<FGameSettingDataSource> Getter;
	TSharedPtr<FGameSettingDataSource> Setter;

	TOptional<double> DefaultValue;
	double InitialValue = 0;

	TRange<double> SourceRange = TRange<double>(0, 1);
	double SourceStep = 0.01;
	TOptional<double> Minimum;
	TOptional<double> Maximum;

	FSettingScalarFormatFunction DisplayFormat;
};

#undef UE_API
