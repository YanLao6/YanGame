// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnhancedInputSubsystems.h"
#include "GameSettingCollection.h"
#include "GameSettingValueDiscreteDynamic.h"
#include "GameSettingValueScalarDynamic.h"
#include "Settings/CustomSettings/YanSettingKeyboardInput.h"
#include "Settings/YanGameSettingRegistry.h"
#include "Settings/YanSettingsShared.h"

#define LOCTEXT_NAMESPACE "Yan"

UGameSettingCollection* UYanGameSettingRegistry::InitializeMouseAndKeyboardSettings(ULocalPlayer* InLocalPlayer)
{
	UGameSettingCollection* Screen = NewObject<UGameSettingCollection>();
	Screen->SetDevName(TEXT("MouseAndKeyboardCollection"));
	Screen->SetDisplayName(LOCTEXT("MouseAndKeyboardCollection_Name", "键鼠"));
	Screen->Initialize(InLocalPlayer);

	// 灵敏度
	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* Sensitivity = NewObject<UGameSettingCollection>();
		Sensitivity->SetDevName(TEXT("MouseSensitivityCollection"));
		Sensitivity->SetDisplayName(LOCTEXT("MouseSensitivityCollection_Name", "灵敏度"));
		Screen->AddSetting(Sensitivity);

		//----------------------------------------------------------------------------------
		{
			UGameSettingValueScalarDynamic* Setting = NewObject<UGameSettingValueScalarDynamic>();
			Setting->SetDevName(TEXT("MouseSensitivityYaw"));
			Setting->SetDisplayName(LOCTEXT("MouseSensitivityYaw_Name", "水平灵敏度"));
			Setting->SetDescriptionRichText(LOCTEXT("MouseSensitivityYaw_Description", "鼠标水平（X）轴的灵敏度。数值越大，左右转动视角越快。"));

			Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH(GetMouseSensitivityX));
			Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH(SetMouseSensitivityX));
			Setting->SetDefaultValue(GetDefault<UYanSettingsShared>()->GetMouseSensitivityX());
			Setting->SetDisplayFormat(UGameSettingValueScalarDynamic::RawTwoDecimals);
			Setting->SetSourceRangeAndStep(TRange<double>(0, 10), 0.01);
			Setting->SetMinimumLimit(0.01);

			Sensitivity->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueScalarDynamic* Setting = NewObject<UGameSettingValueScalarDynamic>();
			Setting->SetDevName(TEXT("MouseSensitivityPitch"));
			Setting->SetDisplayName(LOCTEXT("MouseSensitivityPitch_Name", "垂直灵敏度"));
			Setting->SetDescriptionRichText(LOCTEXT("MouseSensitivityPitch_Description", "鼠标垂直（Y）轴的灵敏度。数值越大，上下转动视角越快。"));

			Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH(GetMouseSensitivityY));
			Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH(SetMouseSensitivityY));
			Setting->SetDefaultValue(GetDefault<UYanSettingsShared>()->GetMouseSensitivityY());
			Setting->SetDisplayFormat(UGameSettingValueScalarDynamic::RawTwoDecimals);
			Setting->SetSourceRangeAndStep(TRange<double>(0, 10), 0.01);
			Setting->SetMinimumLimit(0.01);

			Sensitivity->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueScalarDynamic* Setting = NewObject<UGameSettingValueScalarDynamic>();
			Setting->SetDevName(TEXT("MouseTargetingMultiplier"));
			Setting->SetDisplayName(LOCTEXT("MouseTargetingMultiplier_Name", "瞄准灵敏度倍率"));
			Setting->SetDescriptionRichText(LOCTEXT("MouseTargetingMultiplier_Description", "瞄准时叠加的灵敏度倍率。数值越低，瞄准时视角移动越慢。"));

			Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH(GetTargetingMultiplier));
			Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH(SetTargetingMultiplier));
			Setting->SetDefaultValue(GetDefault<UYanSettingsShared>()->GetTargetingMultiplier());
			Setting->SetDisplayFormat(UGameSettingValueScalarDynamic::RawTwoDecimals);
			Setting->SetSourceRangeAndStep(TRange<double>(0, 10), 0.01);
			Setting->SetMinimumLimit(0.01);

			Sensitivity->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueDiscreteDynamic_Bool* Setting = NewObject<UGameSettingValueDiscreteDynamic_Bool>();
			Setting->SetDevName(TEXT("InvertVerticalAxis"));
			Setting->SetDisplayName(LOCTEXT("InvertVerticalAxis_Name", "反转垂直视角"));
			Setting->SetDescriptionRichText(LOCTEXT("InvertVerticalAxis_Description", "反转视角的垂直移动方向。"));

			Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH(GetInvertVerticalAxis));
			Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH(SetInvertVerticalAxis));
			Setting->SetDefaultValue(GetDefault<UYanSettingsShared>()->GetInvertVerticalAxis());

			Sensitivity->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueDiscreteDynamic_Bool* Setting = NewObject<UGameSettingValueDiscreteDynamic_Bool>();
			Setting->SetDevName(TEXT("InvertHorizontalAxis"));
			Setting->SetDisplayName(LOCTEXT("InvertHorizontalAxis_Name", "反转水平视角"));
			Setting->SetDescriptionRichText(LOCTEXT("InvertHorizontalAxis_Description", "反转视角的水平移动方向。"));

			Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH(GetInvertHorizontalAxis));
			Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH(SetInvertHorizontalAxis));
			Setting->SetDefaultValue(GetDefault<UYanSettingsShared>()->GetInvertHorizontalAxis());

			Sensitivity->AddSetting(Setting);
		}
	}

	// 按键绑定，条目由增强输入的键位配置自动生成
	////////////////////////////////////////////////////////////////////////////////////
	{
		const UEnhancedInputLocalPlayerSubsystem* EISubsystem = InLocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		const UEnhancedInputUserSettings* UserSettings = EISubsystem ? EISubsystem->GetUserSettings() : nullptr;
		if (!UserSettings)
		{
			return Screen;
		}

		TMap<FString, UGameSettingCollection*> CategoryToSettingCollection;

		// 按映射自带的展示分类归组，没有分类的归入默认组
		auto GetOrCreateSettingCollection = [&CategoryToSettingCollection, &Screen](FText DisplayCategory) -> UGameSettingCollection*
			{
				static const FText DefaultDevDisplayName = NSLOCTEXT("YanInputSettings", "YanInputDefaults", "默认");

				if (DisplayCategory.IsEmpty())
				{
					DisplayCategory = DefaultDevDisplayName;
				}

				FString DisplayCatString = DisplayCategory.ToString();

				if (UGameSettingCollection** ExistingCategory = CategoryToSettingCollection.Find(DisplayCatString))
				{
					return *ExistingCategory;
				}

				UGameSettingCollection* ConfigSettingCollection = NewObject<UGameSettingCollection>();
				ConfigSettingCollection->SetDevName(FName(DisplayCatString));
				ConfigSettingCollection->SetDisplayName(DisplayCategory);
				Screen->AddSetting(ConfigSettingCollection);
				CategoryToSettingCollection.Add(DisplayCatString, ConfigSettingCollection);

				return ConfigSettingCollection;
			};

		for (const TPair<FString, TObjectPtr<UEnhancedPlayerMappableKeyProfile>>& ProfilePair : UserSettings->GetAllAvailableKeyProfiles())
		{
			const TObjectPtr<UEnhancedPlayerMappableKeyProfile>& Profile = ProfilePair.Value;

			for (const TPair<FName, FKeyMappingRow>& RowPair : Profile->GetPlayerMappingRows())
			{
				if (!RowPair.Value.HasAnyMappings())
				{
					continue;
				}

				// 本界面只处理键鼠绑定，以一个普通键盘键作为匹配样板过滤掉手柄映射
				FPlayerMappableKeyQueryOptions Options = {};
				Options.KeyToMatch = EKeys::W;
				Options.bMatchBasicKeyTypes = true;

				const FText& DesiredDisplayCategory = RowPair.Value.Mappings.begin()->GetDisplayCategory();

				if (UGameSettingCollection* Collection = GetOrCreateSettingCollection(DesiredDisplayCategory))
				{
					UYanSettingKeyboardInput* InputBinding = NewObject<UYanSettingKeyboardInput>();

					InputBinding->InitializeInputData(Profile, RowPair.Value, Options);

					Collection->AddSetting(InputBinding);
				}
			}
		}
	}

	return Screen;
}

#undef LOCTEXT_NAMESPACE
