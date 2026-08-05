// Copyright Chronicler.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ModularLayoutInterface.generated.h"

class UModularGameplayLayout;

/** IModularLayoutInterface 的 UObject 侧接口壳（UMG / Reflection 用）。 */
UINTERFACE(MinimalAPI)
class UModularLayoutInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 布局访问接口。
 *
 * 用于让 HUD/Widget 获取或设置 `UModularGameplayLayout`，
 * 以便与 GameFeature 注入流程协同。
 */
class IModularLayoutInterface
{
	GENERATED_BODY()

public:
	/** 获取当前布局实例。 */
	virtual UModularGameplayLayout* GetModularGameplayLayout() const = 0;
	/** 设置当前布局实例。 */
	virtual void                    SetModularGameplayLayout(UModularGameplayLayout* InLayout) = 0;
};
