// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/YanSimulatedInputWidget.h"

#include "YanJoystickWidget.generated.h"

class UImage;
class UObject;
struct FGeometry;
struct FPointerEvent;

UCLASS(meta=( DisplayName="Yan Joystick" ))
class YANGAMEUI_API UYanJoystickWidget : public UYanSimulatedInputWidget
{
	GENERATED_BODY()
	
public:
	
	UYanJoystickWidget(const FObjectInitializer& ObjectInitializer);

	//~Begin UUserWidget Interface
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	//~End UUserWidget Interface

	//~Begin UUserWidget Interface
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	//~End UUserWidget Interface

	//~Begin UUserWidget Interface
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	//~End UUserWidget Interface

	//~Begin UUserWidget Interface
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	//~End UUserWidget Interface

	//~Begin UUserWidget Interface
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~End UUserWidget Interface

protected:

	void HandleTouchDelta(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent);

	void StopInputSimulation();

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Yan|UI")
	float StickRange = 50.0f;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Yan|UI")
	TObjectPtr<UImage> JoystickBackground;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Yan|UI")
	TObjectPtr<UImage> JoystickForeground;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Yan|UI")
	bool bNegateYAxis = false;

	UPROPERTY(Transient)
	FVector2D TouchOrigin = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D StickVector = FVector2D::ZeroVector;
};

