// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserWidget.h"
#include "YanSimulatedInputWidget.generated.h"

class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UCommonHardwareVisibilityBorder;
class UEnhancedPlayerInput;

/** 触屏控件的 Enhanced Input 注入与映射生命周期。 */
UCLASS(meta=( DisplayName="Yan Simulated Input Widget" ))
class YANGAMEUI_API UYanSimulatedInputWidget : public UCommonUserWidget
{
	GENERATED_BODY()
	
public:
	
	UYanSimulatedInputWidget(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR

	//~Begin UWidget Interface
	virtual const FText GetPaletteCategory() override;
	//~End UWidget Interface

#endif

	//~Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	//~End UUserWidget Interface

	//~Begin UUserWidget Interface
	virtual void NativeDestruct() override;
	//~End UUserWidget Interface

	//~Begin UUserWidget Interface
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	//~End UUserWidget Interface

	/** 取得控件所属 LocalPlayer 的 Enhanced Input 子系统。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|UI")
	UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;

	UEnhancedPlayerInput* GetPlayerInput() const;

	/** 取得触屏输入所关联的 InputAction。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|UI")
	const UInputAction* GetAssociatedAction() const { return AssociatedAction; }

	/** 取得当前用于模拟输入的按键。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|UI")
	FKey GetSimulatedKey() const { return KeyToSimulate; }

	/** 优先注入 InputAction；未关联 Action 时模拟映射按键。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|UI")
	void InputKeyValue(const FVector& Value);

	/** 提交二维触屏输入。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|UI")
	void InputKeyValue2D(const FVector2D& Value);

	/** 释放模拟输入的按键状态。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|UI")
	void FlushSimulatedInput();
	
protected:

	void QueryKeyToSimulate();

	UFUNCTION()
	void OnControlMappingsRebuilt();

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Yan|UI")
	TObjectPtr<UCommonHardwareVisibilityBorder> CommonVisibilityBorder = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yan|UI")
	TObjectPtr<const UInputAction> AssociatedAction = nullptr;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Yan|UI")
	FKey FallbackBindingKey = EKeys::Gamepad_Right2D;

	FKey KeyToSimulate;
};

