// Copyright Chronicler.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * ModularGameplayExperiences 插件模块。
 *
 * @todo 当前设计仍依赖在 Project Settings 中大量覆盖默认类，属于反模式，宜逐步收敛。
 */
class FModularGameplayExperiencesModule final : public IModuleInterface
{
public:

	/** IModuleInterface：模块启动。 */
	virtual void StartupModule() override;
	/** IModuleInterface：模块关闭。 */
	virtual void ShutdownModule() override;
};
