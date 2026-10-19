// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/CommonBoundActionButton.h"

#include "ModularBoundActionButton.generated.h"

#define UE_API MODULARGAMEPLAYUI_API

class UCommonButtonStyle;
class UObject;

/** 底部操作栏按钮：随当前输入方式切换按键提示样式 */
UCLASS(MinimalAPI, Abstract, meta = (DisableNativeTick))
class UModularBoundActionButton : public UCommonBoundActionButton
{
	GENERATED_BODY()

protected:
	//~Begin UUserWidget Interface
	UE_API virtual void NativeConstruct() override;
	//~End UUserWidget Interface

private:
	void HandleInputMethodChanged(ECommonInputType NewInputMethod);

	UPROPERTY(EditAnywhere, Category = "Styles")
	TSubclassOf<UCommonButtonStyle> KeyboardStyle;

	UPROPERTY(EditAnywhere, Category = "Styles")
	TSubclassOf<UCommonButtonStyle> GamepadStyle;

	UPROPERTY(EditAnywhere, Category = "Styles")
	TSubclassOf<UCommonButtonStyle> TouchStyle;
};

#undef UE_API
