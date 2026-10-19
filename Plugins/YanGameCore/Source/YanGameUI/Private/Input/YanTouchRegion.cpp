// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/YanTouchRegion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanTouchRegion)

struct FGeometry;
struct FPointerEvent;

FReply UYanTouchRegion::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	bShouldSimulateInput = true;
	return Super::NativeOnTouchStarted(InGeometry, InGestureEvent);
}

FReply UYanTouchRegion::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{

	bShouldSimulateInput = true;
	
	return Super::NativeOnTouchMoved(InGeometry, InGestureEvent);
}

FReply UYanTouchRegion::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	bShouldSimulateInput = false;
	return Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
}

void UYanTouchRegion::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if(bShouldSimulateInput)
	{
		InputKeyValue(FVector::OneVector);
	}
}

