// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/CustomSettings/YanSettingKeyboardInput.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanSettingKeyboardInput)

#define LOCTEXT_NAMESPACE "YanSettings"

namespace Yan::ErrorMessages
{
	static const FText UnknownMappingName = LOCTEXT("YanErrors_UnknownMappingName", "未知绑定");
}

UYanSettingKeyboardInput::UYanSettingKeyboardInput()
{
	bReportAnalytics = false;
}

FText UYanSettingKeyboardInput::GetSettingDisplayName() const
{
	if (const FKeyMappingRow* Row = FindKeyMappingRow())
	{
		if (Row->HasAnyMappings())
		{
			return Row->Mappings.begin()->GetDisplayName();
		}
	}

	return Yan::ErrorMessages::UnknownMappingName;
}

FText UYanSettingKeyboardInput::GetSettingDisplayCategory() const
{
	if (const FKeyMappingRow* Row = FindKeyMappingRow())
	{
		if (Row->HasAnyMappings())
		{
			return Row->Mappings.begin()->GetDisplayCategory();
		}
	}

	return Yan::ErrorMessages::UnknownMappingName;
}

const FKeyMappingRow* UYanSettingKeyboardInput::FindKeyMappingRow() const
{
	if (const UEnhancedPlayerMappableKeyProfile* Profile = FindMappableKeyProfile())
	{
		return Profile->FindKeyMappingRow(ActionMappingName);
	}

	ensure(false);
	return nullptr;
}

UEnhancedPlayerMappableKeyProfile* UYanSettingKeyboardInput::FindMappableKeyProfile() const
{
	if (UEnhancedInputUserSettings* Settings = GetUserSettings())
	{
		return Settings->GetKeyProfileWithId(ProfileIdentifier);
	}

	ensure(false);
	return nullptr;
}

UEnhancedInputUserSettings* UYanSettingKeyboardInput::GetUserSettings() const
{
	if (LocalPlayer)
	{
		if (UEnhancedInputLocalPlayerSubsystem* System = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			return System->GetUserSettings();
		}
	}

	return nullptr;
}

void UYanSettingKeyboardInput::OnInitialized()
{
	DynamicDetails = FGetGameSettingsDetails::CreateLambda([this](ULocalPlayer&)
		{
			if (const FKeyMappingRow* Row = FindKeyMappingRow())
			{
				if (Row->HasAnyMappings())
				{
					return FText::Format(LOCTEXT("DynamicDetails_KeyboardInputAction", "{0} 的按键绑定"), Row->Mappings.begin()->GetDisplayName());
				}
			}

			return FText::GetEmpty();
		});

	Super::OnInitialized();
}

void UYanSettingKeyboardInput::InitializeInputData(const UEnhancedPlayerMappableKeyProfile* KeyProfile, const FKeyMappingRow& MappingData, const FPlayerMappableKeyQueryOptions& InQueryOptions)
{
	check(KeyProfile);

	ProfileIdentifier = KeyProfile->GetProfileIdString();
	QueryOptions = InQueryOptions;

	for (const FPlayerKeyMapping& Mapping : MappingData.Mappings)
	{
		// 只接纳通过筛选条件的映射
		if (!KeyProfile->DoesMappingPassQueryOptions(Mapping, QueryOptions))
		{
			continue;
		}

		ActionMappingName = Mapping.GetMappingName();
		InitialKeyMappings.Add(Mapping.GetSlot(), Mapping.GetCurrentKey());

		const FText& MappingDisplayName = Mapping.GetDisplayName();
		if (!MappingDisplayName.IsEmpty())
		{
			SetDisplayName(MappingDisplayName);
		}
	}

	const FString NameString = TEXT("KBM_Input_") + ActionMappingName.ToString();
	SetDevName(*NameString);
}

FText UYanSettingKeyboardInput::GetKeyTextFromSlot(const EPlayerMappableKeySlot InSlot) const
{
	if (const UEnhancedPlayerMappableKeyProfile* Profile = FindMappableKeyProfile())
	{
		FPlayerMappableKeyQueryOptions QueryOptionsForSlot = QueryOptions;
		QueryOptionsForSlot.SlotToMatch = InSlot;

		if (const FKeyMappingRow* Row = FindKeyMappingRow())
		{
			for (const FPlayerKeyMapping& Mapping : Row->Mappings)
			{
				if (Profile->DoesMappingPassQueryOptions(Mapping, QueryOptionsForSlot))
				{
					return Mapping.GetCurrentKey().GetDisplayName();
				}
			}
		}
	}

	return EKeys::Invalid.GetDisplayName();
}

void UYanSettingKeyboardInput::ResetToDefault()
{
	if (UEnhancedInputUserSettings* Settings = GetUserSettings())
	{
		FMapPlayerKeyArgs Args = {};
		Args.MappingName = ActionMappingName;

		FGameplayTagContainer FailureReason;
		Settings->ResetAllPlayerKeysInRow(Args, FailureReason);

		NotifySettingChanged(EGameSettingChangeReason::Change);
	}
}

void UYanSettingKeyboardInput::StoreInitial()
{
	if (const UEnhancedPlayerMappableKeyProfile* Profile = FindMappableKeyProfile())
	{
		if (const FKeyMappingRow* Row = FindKeyMappingRow())
		{
			for (const FPlayerKeyMapping& Mapping : Row->Mappings)
			{
				if (Profile->DoesMappingPassQueryOptions(Mapping, QueryOptions))
				{
					ActionMappingName = Mapping.GetMappingName();
					InitialKeyMappings.Add(Mapping.GetSlot(), Mapping.GetCurrentKey());
				}
			}
		}
	}
}

void UYanSettingKeyboardInput::RestoreToInitial()
{
	for (const TPair<EPlayerMappableKeySlot, FKey>& Pair : InitialKeyMappings)
	{
		ChangeBinding((int32)Pair.Key, Pair.Value);
	}
}

bool UYanSettingKeyboardInput::ChangeBinding(int32 InKeyBindSlot, FKey NewKey)
{
	if (NewKey.IsGamepadKey())
	{
		return false;
	}

	FMapPlayerKeyArgs Args = {};
	Args.MappingName = ActionMappingName;
	Args.Slot = (EPlayerMappableKeySlot)(static_cast<uint8>(InKeyBindSlot));
	Args.NewKey = NewKey;

	if (UEnhancedInputUserSettings* Settings = GetUserSettings())
	{
		FGameplayTagContainer FailureReason;
		Settings->MapPlayerKey(Args, FailureReason);
		NotifySettingChanged(EGameSettingChangeReason::Change);
	}

	return true;
}

void UYanSettingKeyboardInput::GetAllMappedActionsFromKey(int32 InKeyBindSlot, FKey Key, TArray<FName>& OutActionNames) const
{
	if (const UEnhancedPlayerMappableKeyProfile* Profile = FindMappableKeyProfile())
	{
		Profile->GetMappingNamesForKey(Key, OutActionNames);
	}
}

bool UYanSettingKeyboardInput::IsMappingCustomized() const
{
	bool bResult = false;

	if (const UEnhancedPlayerMappableKeyProfile* Profile = FindMappableKeyProfile())
	{
		if (const FKeyMappingRow* Row = FindKeyMappingRow())
		{
			for (const FPlayerKeyMapping& Mapping : Row->Mappings)
			{
				if (Profile->DoesMappingPassQueryOptions(Mapping, QueryOptions))
				{
					bResult |= Mapping.IsCustomized();
				}
			}
		}
	}

	return bResult;
}

#undef LOCTEXT_NAMESPACE
