// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/YanSimulatedInputWidget.h"

#include "YanTouchRegion.generated.h"

class UObject;
struct FFrame;
struct FGeometry;
struct FPointerEvent;

UCLASS(meta=( DisplayName="Lyra Touch Region" ))
class YANGAMEUI_API UYanTouchRegion : public UYanSimulatedInputWidget
{
	GENERATED_BODY()
	
public:

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
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~End UUserWidget Interface

	UFUNCTION(BlueprintCallable, Category = "Yan|UI")
	bool ShouldSimulateInput() const { return bShouldSimulateInput; }

protected:

	bool bShouldSimulateInput = false;
};

