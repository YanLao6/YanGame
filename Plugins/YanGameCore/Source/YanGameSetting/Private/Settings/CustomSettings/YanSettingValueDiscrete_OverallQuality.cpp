// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/CustomSettings/YanSettingValueDiscrete_OverallQuality.h"

#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanSettingValueDiscrete_OverallQuality)

#define LOCTEXT_NAMESPACE "YanSettings"

void UYanSettingValueDiscrete_OverallQuality::OnInitialized()
{
	Super::OnInitialized();

	Options.Add(LOCTEXT("VideoQualityOverall_Low", "低"));
	Options.Add(LOCTEXT("VideoQualityOverall_Medium", "中"));
	Options.Add(LOCTEXT("VideoQualityOverall_High", "高"));
	Options.Add(LOCTEXT("VideoQualityOverall_Epic", "极高"));

	OptionsWithCustom = Options;
	OptionsWithCustom.Add(LOCTEXT("VideoQualityOverall_Custom", "自定义"));
}

void UYanSettingValueDiscrete_OverallQuality::StoreInitial()
{
}

void UYanSettingValueDiscrete_OverallQuality::ResetToDefault()
{
}

void UYanSettingValueDiscrete_OverallQuality::RestoreToInitial()
{
}

void UYanSettingValueDiscrete_OverallQuality::SetDiscreteOptionByIndex(int32 Index)
{
	UGameUserSettings* UserSettings = CastChecked<UGameUserSettings>(GEngine->GetGameUserSettings());

	// 选中"自定义"时不改动任何档位，保持玩家逐项调整的结果
	if (Index != GetCustomOptionIndex())
	{
		UserSettings->SetOverallScalabilityLevel(Index);
	}

	NotifySettingChanged(EGameSettingChangeReason::Change);
}

int32 UYanSettingValueDiscrete_OverallQuality::GetDiscreteOptionIndex() const
{
	const int32 OverallQualityLevel = GetOverallQualityLevel();
	if (OverallQualityLevel == INDEX_NONE)
	{
		return GetCustomOptionIndex();
	}

	return OverallQualityLevel;
}

TArray<FText> UYanSettingValueDiscrete_OverallQuality::GetDiscreteOptions() const
{
	// 只有确实处于自定义状态时才把该档位暴露出来
	return (GetOverallQualityLevel() == INDEX_NONE) ? OptionsWithCustom : Options;
}

int32 UYanSettingValueDiscrete_OverallQuality::GetCustomOptionIndex() const
{
	return OptionsWithCustom.Num() - 1;
}

int32 UYanSettingValueDiscrete_OverallQuality::GetOverallQualityLevel() const
{
	const UGameUserSettings* UserSettings = CastChecked<const UGameUserSettings>(GEngine->GetGameUserSettings());
	return UserSettings->GetOverallScalabilityLevel();
}

#undef LOCTEXT_NAMESPACE
