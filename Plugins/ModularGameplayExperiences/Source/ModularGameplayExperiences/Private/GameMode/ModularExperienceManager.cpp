// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameMode/ModularExperienceManager.h"
#include "Engine/Engine.h"
#include "Subsystems/SubsystemCollection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularExperienceManager)

#if WITH_EDITOR

void UModularExperienceManager::OnPlayInEditorBegun()
{
	ensure(GameFeaturePluginRequestCountMap.IsEmpty());
	GameFeaturePluginRequestCountMap.Empty();
}

void UModularExperienceManager::NotifyOfPluginActivation(const FString PluginURL)
{
	if (GIsEditor)
	{
		UModularExperienceManager* ExperienceManagerSubsystem = GEngine->GetEngineSubsystem<UModularExperienceManager>();
		check(ExperienceManagerSubsystem);

		// 统计激活该 GameFeature Plugin 的请求方数量；并发加载由子系统处理，此处仅引用计数
		int32& Count = ExperienceManagerSubsystem->GameFeaturePluginRequestCountMap.FindOrAdd(PluginURL);
		++Count;
	}
}

bool UModularExperienceManager::RequestToDeactivatePlugin(const FString PluginURL)
{
	if (GIsEditor)
	{
		UModularExperienceManager* ExperienceManagerSubsystem = GEngine->GetEngineSubsystem<UModularExperienceManager>();
		check(ExperienceManagerSubsystem);

		// 仅当计数归零时允许真正 Deactivate（FILO）
		int32& Count = ExperienceManagerSubsystem->GameFeaturePluginRequestCountMap.FindChecked(PluginURL);
		--Count;

		if (Count == 0)
		{
			ExperienceManagerSubsystem->GameFeaturePluginRequestCountMap.Remove(PluginURL);
			return true;
		}

		return false;
	}

	return true;
}

#endif
