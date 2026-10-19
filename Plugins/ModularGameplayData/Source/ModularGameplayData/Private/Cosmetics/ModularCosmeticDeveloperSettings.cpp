// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cosmetics/ModularCosmeticDeveloperSettings.h"

#include "Cosmetics/ModularControllerComponent_CharacterParts.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/App.h"
#include "TimerManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularCosmeticDeveloperSettings)

#define LOCTEXT_NAMESPACE "ModularCosmetics"

UModularCosmeticDeveloperSettings::UModularCosmeticDeveloperSettings()
{}

FName UModularCosmeticDeveloperSettings::GetCategoryName() const
{
	return FApp::GetProjectName();
}

#if WITH_EDITOR

void UModularCosmeticDeveloperSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplySettings();
}

void UModularCosmeticDeveloperSettings::PostReloadConfig(FProperty* PropertyThatWasLoaded)
{
	Super::PostReloadConfig(PropertyThatWasLoaded);

	ApplySettings();
}

void UModularCosmeticDeveloperSettings::PostInitProperties()
{
	Super::PostInitProperties();

	ApplySettings();
}

void UModularCosmeticDeveloperSettings::ApplySettings()
{
	if (GIsEditor && (GEngine != nullptr))
	{
		ReapplyLoadoutIfInPIE();
	}
}

void UModularCosmeticDeveloperSettings::ReapplyLoadoutIfInPIE()
{
#if WITH_SERVER_CODE
	check(GEngine);

	// 找出 PIE 中具备权威的世界：专用服务器最理想，否则取 NetMode 最靠前的一个
	UWorld* ServerWorld = nullptr;
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		if (WorldContext.WorldType != EWorldType::PIE)
		{
			continue;
		}

		UWorld* TestWorld = WorldContext.World();
		if (!TestWorld)
		{
			continue;
		}

		if (WorldContext.RunAsDedicated)
		{
			ServerWorld = TestWorld;
			break;
		}

		if (ServerWorld == nullptr || TestWorld->GetNetMode() < ServerWorld->GetNetMode())
		{
			ServerWorld = TestWorld;
		}
	}

	if (ServerWorld == nullptr)
	{
		return;
	}

	// 属性变更可能发生在 Actor 迭代过程中，推迟到下一帧再下发
	ServerWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([ServerWorld]()
	{
		for (TActorIterator<APlayerController> PCIterator(ServerWorld); PCIterator; ++PCIterator)
		{
			if (const APlayerController* PC = *PCIterator)
			{
				if (UModularControllerComponent_CharacterParts* CosmeticComponent = PC->FindComponentByClass<UModularControllerComponent_CharacterParts>())
				{
					CosmeticComponent->ApplyDeveloperSettings();
				}
			}
		}
	}));
#endif // WITH_SERVER_CODE
}

void UModularCosmeticDeveloperSettings::OnPlayInEditorStarted() const
{
	// 外观覆盖生效时给出提示，避免误以为是资产问题
	if (CheatCosmeticCharacterParts.Num() > 0)
	{
		FNotificationInfo Info(LOCTEXT("CosmeticOverrideActive", "正在应用外观覆盖"));
		Info.ExpireDuration = 2.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
