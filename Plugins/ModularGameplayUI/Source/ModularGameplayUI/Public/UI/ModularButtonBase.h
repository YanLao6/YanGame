// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonButtonBase.h"

#include "ModularButtonBase.generated.h"

class UObject;
struct FFrame;

/**
 * Modular 按钮基类：随 Input Method（键盘/手柄等）或其它触发刷新文案与样式。
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class MODULARGAMEPLAYUI_API UModularButtonBase : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/** 设置按钮显示文本（可与 InputAction 显示文本协同）。 */
	UFUNCTION(BlueprintCallable)
	void SetButtonText(const FText& InText);
	
protected:
	// UUserWidget 接口
	virtual void NativePreConstruct() override;

	// UCommonButtonBase 接口
	virtual void UpdateInputActionWidget() override;
	virtual void OnInputMethodChanged(ECommonInputType CurrentInputType) override;

	// 根据 Override 与 InputAction 刷新最终文案
	void RefreshButtonText();
	
	/** Blueprint 实现：应用最终 FText 到可视化文案。 */
	UFUNCTION(BlueprintImplementableEvent)
	void UpdateButtonText(const FText& InText);

	/** Blueprint 实现：输入方式变化时更新样式。 */
	UFUNCTION(BlueprintImplementableEvent)
	void UpdateButtonStyle();
	
private:
	UPROPERTY(EditAnywhere, Category="Button", meta=(InlineEditConditionToggle))
	uint8 bOverride_ButtonText : 1;
	
	UPROPERTY(EditAnywhere, Category="Button", meta=( editcondition="bOverride_ButtonText" ))
	FText ButtonText;
};
