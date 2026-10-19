#include "Settings/CustomSettings/YanSettingValueDiscrete_Language.h"

#include "Internationalization/Culture.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Messaging/CommonGameDialog.h"
#include "Messaging/CommonMessagingSubsystem.h"
#include "Settings/YanSettingsSubsystem.h"
#include "Settings/YanSettingsShared.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanSettingValueDiscrete_Language)

#define LOCTEXT_NAMESPACE "YanSettings"

namespace
{
	// 索引 0 恒为「跟随系统默认」
	constexpr int32 SettingSystemDefaultLanguageIndex = 0;
}

UYanSettingValueDiscrete_Language::UYanSettingValueDiscrete_Language()
{}

void UYanSettingValueDiscrete_Language::OnInitialized()
{
	Super::OnInitialized();

	const TArray<FString> AllCultureNames = FTextLocalizationManager::Get().GetLocalizedCultureNames(ELocalizationLoadFlags::Game);
	for (const FString& CultureName : AllCultureNames)
	{
		if (FInternationalization::Get().IsCultureAllowed(CultureName))
		{
			AvailableCultureNames.Add(CultureName);
		}
	}

	AvailableCultureNames.Insert(TEXT(""), SettingSystemDefaultLanguageIndex);
}

void UYanSettingValueDiscrete_Language::StoreInitial()
{
	// 初始值由 GGameUserSettingsIni 持有，无需在此额外备份
}

void UYanSettingValueDiscrete_Language::OnApply()
{
	if (UCommonMessagingSubsystem* Messaging = LocalPlayer->GetSubsystem<UCommonMessagingSubsystem>())
	{
		Messaging->ShowConfirmation(
			UCommonGameDialogDescriptor::CreateConfirmationOk(
				LOCTEXT("WarningLanguage_Title", "语言已更改"),
				LOCTEXT("WarningLanguage_Message", "需要完全重启游戏，语言相关的更改才会全部生效。")));
	}
}

void UYanSettingValueDiscrete_Language::ResetToDefault()
{
	SetDiscreteOptionByIndex(SettingSystemDefaultLanguageIndex);
}

void UYanSettingValueDiscrete_Language::RestoreToInitial()
{
	if (UYanSettingsShared* Settings = UYanSettingsSubsystem::GetSharedSettingsFor(LocalPlayer))
	{
		Settings->ClearPendingCulture();
		NotifySettingChanged(EGameSettingChangeReason::RestoreToInitial);
	}
}

void UYanSettingValueDiscrete_Language::SetDiscreteOptionByIndex(int32 Index)
{
	if (UYanSettingsShared* Settings = UYanSettingsSubsystem::GetSharedSettingsFor(LocalPlayer))
	{
		if (Index == SettingSystemDefaultLanguageIndex)
		{
			Settings->ResetToDefaultCulture();
		}
		else if (AvailableCultureNames.IsValidIndex(Index))
		{
			Settings->SetPendingCulture(AvailableCultureNames[Index]);
		}

		NotifySettingChanged(EGameSettingChangeReason::Change);
	}
}

int32 UYanSettingValueDiscrete_Language::GetDiscreteOptionIndex() const
{
	const UYanSettingsShared* Settings = UYanSettingsSubsystem::GetSharedSettingsFor(LocalPlayer);
	if (!Settings)
	{
		return 0;
	}

	if (Settings->ShouldResetToDefaultCulture())
	{
		return SettingSystemDefaultLanguageIndex;
	}

	// 优先取待应用语言：UI 在应用之前就要反映用户的选择
	FString PendingCulture = Settings->GetPendingCulture();
	if (PendingCulture.IsEmpty())
	{
		if (Settings->IsUsingDefaultCulture())
		{
			return SettingSystemDefaultLanguageIndex;
		}

		PendingCulture = FInternationalization::Get().GetCurrentCulture()->GetName();
	}

	const int32 ExactMatchIndex = AvailableCultureNames.IndexOfByKey(PendingCulture);
	if (ExactMatchIndex != INDEX_NONE)
	{
		return ExactMatchIndex;
	}

	// 退而求其次找优先级匹配，使 "en-US" 能落到界面上的 "en"
	const TArray<FString> PrioritizedPendingCultures = FInternationalization::Get().GetPrioritizedCultureNames(PendingCulture);
	for (int32 i = 0; i < AvailableCultureNames.Num(); ++i)
	{
		if (PrioritizedPendingCultures.Contains(AvailableCultureNames[i]))
		{
			return i;
		}
	}

	return 0;
}

TArray<FText> UYanSettingValueDiscrete_Language::GetDiscreteOptions() const
{
	TArray<FText> Options;

	for (const FString& CultureName : AvailableCultureNames)
	{
		if (CultureName.IsEmpty())
		{
			const FCulturePtr SystemDefaultCulture = FInternationalization::Get().GetDefaultCulture();
			if (ensure(SystemDefaultCulture))
			{
				Options.Add(FText::Format(
					LOCTEXT("SystemDefaultLanguage", "系统默认（{0}）"),
					FText::FromString(SystemDefaultCulture->GetDisplayName())));
			}
		}
		else
		{
			const FCulturePtr Culture = FInternationalization::Get().GetCulture(CultureName);
			if (ensureMsgf(Culture, TEXT("找不到语言 '%s'"), *CultureName))
			{
				const FString CultureDisplayName = Culture->GetDisplayName();
				const FString CultureNativeName  = Culture->GetNativeName();

				// 两个名称相同时只显示一个，避免重复
				const FString Entry = !CultureNativeName.Equals(CultureDisplayName, ESearchCase::CaseSensitive)
					                      ? FString::Printf(TEXT("%s (%s)"), *CultureNativeName, *CultureDisplayName)
					                      : CultureNativeName;

				Options.Add(FText::FromString(Entry));
			}
		}
	}

	return Options;
}

#undef LOCTEXT_NAMESPACE
