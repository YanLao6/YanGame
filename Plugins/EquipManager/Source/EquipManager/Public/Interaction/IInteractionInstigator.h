// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "InteractionOption.h"
#include "IInteractionInstigator.generated.h"

struct FInteractionQuery;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UInteractionInstigator : public UInterface
{
	GENERATED_BODY()
};

/**
 * 实现本接口可为交互过程加入一个仲裁者。例如某些游戏会向玩家弹出菜单选择要执行的交互，
 * 当交互能力生成多个选项时，可借此从多个匹配项中挑选最合适的一个。
 */
class IInteractionInstigator
{
	GENERATED_BODY()

public:
	/** 当存在多个交互选项需要抉择时调用。 */
	virtual FInteractionOption ChooseBestInteractionOption(const FInteractionQuery& InteractQuery, const TArray<FInteractionOption>& InteractOptions) = 0;
};
