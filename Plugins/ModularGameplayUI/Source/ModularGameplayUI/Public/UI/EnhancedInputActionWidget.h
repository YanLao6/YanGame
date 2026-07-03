// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonActionWidget.h"
#include "EnhancedInputActionWidget.generated.h"

class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;

/**
 * EnhancedInput 动作图标控件。
 *
 * 根据当前输入映射，返回与 `AssociatedInputAction` 对应的按键图标。
 */
UCLASS(BlueprintType, Blueprintable)
class UEnhancedInputActionWidget : public UCommonActionWidget
{
	GENERATED_BODY()

public:

	//~ UCommonActionWidget 接口
	/** 返回当前应显示的输入图标。 */
	virtual FSlateBrush GetIcon() const override;
	//~

	/** 与该 CommonAction 绑定的 EnhancedInput Action。 */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	const TObjectPtr<UInputAction> AssociatedInputAction;

private:

	// 获取本地玩家的 EnhancedInput 子系统。
	UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;
	
};