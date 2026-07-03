// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/EngineSubsystem.h"
#include "ModularExperienceManager.generated.h"

/**
 * Experience 全局管理器。
 *
 * 主要用于 Editor/PIE 多实例下的 GameFeature 激活仲裁与引用计数管理。
 */
UCLASS(MinimalAPI)
class UModularExperienceManager : public UEngineSubsystem
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	/** PIE 开始时初始化内部状态。 */
	MODULARGAMEPLAYEXPERIENCES_API void OnPlayInEditorBegun();

	/** 通知管理器某个插件已被请求激活。 */
	static void NotifyOfPluginActivation(const FString PluginURL);
	/** 请求停用插件；若仍有引用则返回 false。 */
	static bool RequestToDeactivatePlugin(const FString PluginURL);
#else
	static void NotifyOfPluginActivation(const FString PluginURL) {}
	static bool RequestToDeactivatePlugin(const FString PluginURL) { return true; }
#endif

private:
	// GameFeature Plugin URL -> 激活引用计数（PIE 多实例下的 FILO 管理）
	TMap<FString, int32> GameFeaturePluginRequestCountMap;
};
