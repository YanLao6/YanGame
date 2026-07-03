// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * ModularGameplayUI 模块：围绕为游戏提供可扩展的 UIStateComponent Blueprint 类而设计。
 *
 * 可派生该类以跟踪运行时需要状态的 Widget，相当于避免把大量 Widget 引用直接堆在 GameState 上。
 *
 * 若需更细粒度地控制菜单出现时机与生命周期，可在运行时通过 GameFeatureAction_AddComponents
 * 向 ModularExperienceGameState 注入菜单状态组件。
 *
 * 例如在 Blueprint 「UISC_MyGameMenuState」中配置要纳入组件状态的 Widget class。
 *
 * 插件提供的 Widget 体系侧重响应 User Input Mode 或其它 Driver。
 *
 * @example
 *  - Press Start 界面（示例内含）
 *  - 主界面（示例内含）
 *  - 画质设置菜单
 *  - 无障碍快捷菜单
 *  - 多人邀请菜单
 */
class FModularGameplayUIModule : public IModuleInterface
{
public:

	/** IModuleInterface 实现：模块加载后回调。 */
	virtual void StartupModule() override;
	/** IModuleInterface 实现：模块卸载前回调。 */
	virtual void ShutdownModule() override;
};
