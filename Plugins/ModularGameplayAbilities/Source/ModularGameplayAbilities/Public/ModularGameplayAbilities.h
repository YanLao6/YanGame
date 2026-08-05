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
 * 3. 在 PawnData 的 Fragments 中加入 UPawnDataFragment_AbilitySets 配置要授予的技能，
 *    需要 Tag 阻塞/取消关系时再加入 UPawnDataFragment_TagRelationship。
 * 4. 将该 PawnData 配置为 Experience 的默认值或角色目录中的候选项。
 */
class FModularGameplayAbilitiesModule final : public IModuleInterface
{
public:

	/** IModuleInterface：模块加载后调用。 */
	virtual void StartupModule() override;
	/** IModuleInterface：模块卸载前调用。 */
	virtual void ShutdownModule() override;
};
