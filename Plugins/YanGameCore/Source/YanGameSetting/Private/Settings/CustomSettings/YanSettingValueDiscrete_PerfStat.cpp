#include "Settings/CustomSettings/YanSettingValueDiscrete_PerfStat.h"

#include "CommonUIVisibilitySubsystem.h"
#include "Performance/YanPerformanceSettings.h"
#include "Performance/YanPerformanceStatTypes.h"
#include "Settings/YanSettingsLocal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanSettingValueDiscrete_PerfStat)

#define LOCTEXT_NAMESPACE "YanSettings"

namespace
{
	/** 依据性能配置与当前平台特性，决定某项统计是否可在设置界面出现。 */
	class FGameSettingEditCondition_PerfStatAllowed : public FGameSettingEditCondition
	{
	public:
		static TSharedRef<FGameSettingEditCondition_PerfStatAllowed> Get(EYanDisplayablePerformanceStat Stat)
		{
			return MakeShared<FGameSettingEditCondition_PerfStatAllowed>(Stat);
		}

		explicit FGameSettingEditCondition_PerfStatAllowed(EYanDisplayablePerformanceStat Stat)
			: AssociatedStat(Stat)
		{}

		//~Begin FGameSettingEditCondition Interface
		virtual void GatherEditState(const ULocalPlayer* InLocalPlayer, FGameSettingEditableState& InOutEditState) const override
		{
			const FGameplayTagContainer& VisibilityTags = UCommonUIVisibilitySubsystem::GetChecked(InLocalPlayer)->GetVisibilityTags();

			bool bCanShowStat = false;
			for (const FYanPerformanceStatGroup& Group : GetDefault<UYanPerformanceSettings>()->UserFacingPerformanceStats)
			{
				if (Group.AllowedStats.Contains(AssociatedStat)
					&& (Group.VisibilityQuery.IsEmpty() || Group.VisibilityQuery.Matches(VisibilityTags)))
				{
					bCanShowStat = true;
					break;
				}
			}

			if (!bCanShowStat)
			{
				InOutEditState.Hide(TEXT("该统计项未列入 UYanPerformanceSettings，或被当前平台特性屏蔽"));
			}
		}

		//~End FGameSettingEditCondition Interface

	private:
		EYanDisplayablePerformanceStat AssociatedStat;
	};
}

UYanSettingValueDiscrete_PerfStat::UYanSettingValueDiscrete_PerfStat()
{}

void UYanSettingValueDiscrete_PerfStat::SetStat(EYanDisplayablePerformanceStat InStat)
{
	StatToDisplay = InStat;

	SetDevName(FName(*FString::Printf(TEXT("PerfStat_%d"), static_cast<int32>(StatToDisplay))));
	AddEditCondition(FGameSettingEditCondition_PerfStatAllowed::Get(StatToDisplay));
}

void UYanSettingValueDiscrete_PerfStat::AddMode(FText&& Label, EYanStatDisplayMode Mode)
{
	Options.Emplace(MoveTemp(Label));
	DisplayModes.Add(Mode);
}

void UYanSettingValueDiscrete_PerfStat::OnInitialized()
{
	Super::OnInitialized();

	AddMode(LOCTEXT("PerfStatDisplayMode_None", "关闭"), EYanStatDisplayMode::Hidden);
	AddMode(LOCTEXT("PerfStatDisplayMode_TextOnly", "仅文字"), EYanStatDisplayMode::TextOnly);
	AddMode(LOCTEXT("PerfStatDisplayMode_GraphOnly", "仅曲线图"), EYanStatDisplayMode::GraphOnly);
	AddMode(LOCTEXT("PerfStatDisplayMode_TextAndGraph", "文字与曲线图"), EYanStatDisplayMode::TextAndGraph);
}

void UYanSettingValueDiscrete_PerfStat::StoreInitial()
{
	InitialMode = UYanSettingsLocal::Get()->GetPerfStatDisplayState(StatToDisplay);
}

void UYanSettingValueDiscrete_PerfStat::ResetToDefault()
{
	UYanSettingsLocal::Get()->SetPerfStatDisplayState(StatToDisplay, EYanStatDisplayMode::Hidden);
	NotifySettingChanged(EGameSettingChangeReason::ResetToDefault);
}

void UYanSettingValueDiscrete_PerfStat::RestoreToInitial()
{
	UYanSettingsLocal::Get()->SetPerfStatDisplayState(StatToDisplay, InitialMode);
	NotifySettingChanged(EGameSettingChangeReason::RestoreToInitial);
}

void UYanSettingValueDiscrete_PerfStat::SetDiscreteOptionByIndex(int32 Index)
{
	if (DisplayModes.IsValidIndex(Index))
	{
		UYanSettingsLocal::Get()->SetPerfStatDisplayState(StatToDisplay, DisplayModes[Index]);
	}

	NotifySettingChanged(EGameSettingChangeReason::Change);
}

int32 UYanSettingValueDiscrete_PerfStat::GetDiscreteOptionIndex() const
{
	return DisplayModes.Find(UYanSettingsLocal::Get()->GetPerfStatDisplayState(StatToDisplay));
}

TArray<FText> UYanSettingValueDiscrete_PerfStat::GetDiscreteOptions() const
{
	return Options;
}

#undef LOCTEXT_NAMESPACE
