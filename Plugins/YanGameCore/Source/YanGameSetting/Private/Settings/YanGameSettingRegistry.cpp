// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/YanGameSettingRegistry.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameSettingCollection.h"
#include "GameSettingValueDiscrete.h"
#include "GameSettingValueScalar.h"
#include "Engine/LocalPlayer.h"
#include "Settings/YanSettingsLocal.h"
#include "Settings/YanSettingsShared.h"
#include "Settings/YanSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanGameSettingRegistry)

DEFINE_LOG_CATEGORY(LogYanGameSettingRegistry);

#define LOCTEXT_NAMESPACE "Yan"

UYanGameSettingRegistry* UYanGameSettingRegistry::Get(ULocalPlayer* InLocalPlayer)
{
	UYanGameSettingRegistry* Registry = FindObject<UYanGameSettingRegistry>(InLocalPlayer, TEXT("YanGameSettingRegistry"), EFindObjectFlags::ExactClass);
	if (Registry == nullptr)
	{
		Registry = NewObject<UYanGameSettingRegistry>(InLocalPlayer, TEXT("YanGameSettingRegistry"));
		Registry->Initialize(InLocalPlayer);
	}

	return Registry;
}

bool UYanGameSettingRegistry::IsFinishedInitializing() const
{
	if (Super::IsFinishedInitializing())
	{
		// 共享设置可能仍在异步加载，此时设置树尚不能读到正确的初值
		return UYanSettingsSubsystem::GetSharedSettingsFor(OwningLocalPlayer) != nullptr;
	}

	return false;
}

void UYanGameSettingRegistry::OnInitialize(ULocalPlayer* InLocalPlayer)
{
	VideoSettings = InitializeVideoSettings(InLocalPlayer);
	InitializeVideoSettings_FrameRates(VideoSettings, InLocalPlayer);
	RegisterSetting(VideoSettings);

	MouseAndKeyboardSettings = InitializeMouseAndKeyboardSettings(InLocalPlayer);
	RegisterSetting(MouseAndKeyboardSettings);

	AudioSettings = InitializeAudioSettings(InLocalPlayer);
	RegisterSetting(AudioSettings);

	GameplaySettings = InitializeGameplaySettings(InLocalPlayer);
	RegisterSetting(GameplaySettings);

	// 手柄分类暂无设置项，仅占位以保证界面查询安全；补齐设置项后页签会自动出现
	RegisterEmptyCollection(InLocalPlayer, TEXT("GamepadCollection"), LOCTEXT("GamepadCollection_Name", "手柄"));
}

void UYanGameSettingRegistry::RegisterEmptyCollection(ULocalPlayer* InLocalPlayer, FName DevName, const FText& DisplayName)
{
	UGameSettingCollection* Collection = NewObject<UGameSettingCollection>();
	Collection->SetDevName(DevName);
	Collection->SetDisplayName(DisplayName);
	Collection->Initialize(InLocalPlayer);

	RegisterSetting(Collection);
}

void UYanGameSettingRegistry::SaveChanges()
{
	Super::SaveChanges();

	// 分辨率一类的改动需要经 ApplySettings 生效，该调用同时会间接存盘
	UYanSettingsLocal::Get()->ApplySettings(false);

	if (UYanSettingsShared* SharedSettings = UYanSettingsSubsystem::GetSharedSettingsFor(OwningLocalPlayer))
	{
		SharedSettings->ApplySettings();
		SharedSettings->SaveSettings();
	}
}

#if !UE_BUILD_SHIPPING

// 设置界面尚未搭建时，用它确认设置树的结构、取值与禁用条件是否正确
static void DumpSettingRecursive(UGameSetting* Setting, int32 Depth)
{
	const FString Indent = FString::ChrN(Depth * 2, TEXT(' '));

	FString ValueText;
	if (const UGameSettingValueDiscrete* DiscreteSetting = Cast<UGameSettingValueDiscrete>(Setting))
	{
		const TArray<FText> Options = DiscreteSetting->GetDiscreteOptions();
		const int32 SelectedIndex = DiscreteSetting->GetDiscreteOptionIndex();
		const FString SelectedText = Options.IsValidIndex(SelectedIndex) ? Options[SelectedIndex].ToString() : TEXT("<无效>");
		ValueText = FString::Printf(TEXT(" = %s (%d/%d 项)"), *SelectedText, SelectedIndex, Options.Num());
	}
	else if (const UGameSettingValueScalar* ScalarSetting = Cast<UGameSettingValueScalar>(Setting))
	{
		ValueText = FString::Printf(TEXT(" = %s"), *ScalarSetting->GetFormattedText().ToString());
	}

	FString StateText;
	const FGameSettingEditableState& EditState = Setting->GetEditState();
	if (!EditState.IsVisible())
	{
		StateText = TEXT(" [已隐藏]");
	}
	else if (!EditState.IsEnabled())
	{
		StateText = TEXT(" [已禁用]");
	}

	UE_LOG(LogYanGameSettingRegistry, Display, TEXT("%s%s (%s)%s%s"),
		*Indent, *Setting->GetDisplayName().ToString(), *Setting->GetDevName().ToString(), *ValueText, *StateText);

	for (UGameSetting* Child : Setting->GetChildSettings())
	{
		DumpSettingRecursive(Child, Depth + 1);
	}
}

void UYanGameSettingRegistry::DumpSettings()
{
	UE_LOG(LogYanGameSettingRegistry, Display, TEXT("===== 设置树 (初始化完成: %s) ====="), IsFinishedInitializing() ? TEXT("是") : TEXT("否"));

	for (UGameSetting* Setting : TopLevelSettings)
	{
		DumpSettingRecursive(Setting, 0);
	}
}

static FAutoConsoleCommandWithWorld GDumpYanSettingsCmd(
	TEXT("Yan.Settings.Dump"),
	TEXT("打印当前本地玩家的设置树，用于在设置界面就绪前验证设置项"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (!World || !World->GetGameInstance())
			{
				return;
			}

			if (ULocalPlayer* LocalPlayer = World->GetGameInstance()->GetFirstGamePlayer())
			{
				UYanGameSettingRegistry::Get(LocalPlayer)->DumpSettings();
			}
			else
			{
				UE_LOG(LogYanGameSettingRegistry, Warning, TEXT("未找到本地玩家"));
			}
		}));

#endif

#undef LOCTEXT_NAMESPACE
