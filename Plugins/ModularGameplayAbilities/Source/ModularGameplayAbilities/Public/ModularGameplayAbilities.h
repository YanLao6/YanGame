// Copyright Chronicler.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * 为 Gameplay Ability System（GAS）提供模块化、数据驱动的 Experience 与 Data 能力扩展。
 *
 * 典型接入：
 * 1. 在 PlayerState 上挂载 UModularAbilitySystemComponent。
 * 2. 在 ModularExperienceCharacter（Pawn）上挂载 UModularAbilityExtensionComponent。
 * 3. 在游戏的 PawnData 上实现 IAbilityPawnDataInterface。
 * 4. 像往常一样配置 Experience 的 PawnData，并包含 Ability 相关资产。
 */
class FModularGameplayAbilitiesModule final : public IModuleInterface
{
public:

	/** IModuleInterface：模块加载后调用。 */
	virtual void StartupModule() override;
	/** IModuleInterface：模块卸载前调用。 */
	virtual void ShutdownModule() override;
};
