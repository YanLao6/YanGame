// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/YanJoystickWidget.h"

#include "CommonHardwareVisibilityBorder.h"
#include "Components/Image.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanJoystickWidget)

#define LOCTEXT_NAMESPACE "LyraJoystick"

UYanJoystickWidget::UYanJoystickWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetConsumePointerInput(true);
}

FReply UYanJoystickWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	Super::NativeOnTouchStarted(InGeometry, InGestureEvent);
	
	TouchOrigin = InGestureEvent.GetScreenSpacePosition();

	FReply Reply = FReply::Handled();
	if (!HasMouseCaptureByUser(InGestureEvent.GetUserIndex(), InGestureEvent.GetPointerIndex()))
	{
		Reply.CaptureMouse(GetCachedWidget().ToSharedRef());
	}
	return Reply;
}

FReply UYanJoystickWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	Super::NativeOnTouchMoved(InGeometry, InGestureEvent);
	HandleTouchDelta(InGeometry, InGestureEvent);

	FReply Reply = FReply::Handled();
	if (!HasMouseCaptureByUser(InGestureEvent.GetUserIndex(), InGestureEvent.GetPointerIndex()))
	{
		Reply.CaptureMouse(GetCachedWidget().ToSharedRef());
	}
	return Reply;
}

FReply UYanJoystickWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	StopInputSimulation();
	return FReply::Handled().ReleaseMouseCapture();
}

void UYanJoystickWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	StopInputSimulation();
}

void UYanJoystickWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!CommonVisibilityBorder || CommonVisibilityBorder->IsVisible())
	{
		
		if (JoystickForeground && JoystickBackground)
		{
			JoystickForeground->SetRenderTranslation(
				(bNegateYAxis ? FVector2D(1.0f, -1.0f) : FVector2D(1.0f)) *
				StickVector *
				(JoystickBackground->GetDesiredSize() * 0.5f)
			);
		}
		InputKeyValue2D(StickVector);
	}
}

void UYanJoystickWidget::HandleTouchDelta(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	const FVector2D& ScreenSpacePos = InGestureEvent.GetScreenSpacePosition();

	FVector2D LocalStickCenter = InGeometry.GetLocalSize() * 0.5f;

	FVector2D ScreenSpaceStickCenter = InGeometry.LocalToAbsolute(LocalStickCenter);
	
	FVector2D MoveStickOffset = (ScreenSpacePos - ScreenSpaceStickCenter);
	if (bNegateYAxis)
	{
		MoveStickOffset *= FVector2D(1.0f, -1.0f);
	}
	
	FVector2D MoveStickDir = FVector2D::ZeroVector;
	float MoveStickLength = 0.0f;
	MoveStickOffset.ToDirectionAndLength(MoveStickDir, MoveStickLength);

	MoveStickLength = FMath::Min(MoveStickLength, StickRange);
	MoveStickOffset = MoveStickDir * MoveStickLength;

	StickVector = StickRange > UE_SMALL_NUMBER ? MoveStickOffset / StickRange : FVector2D::ZeroVector;
}

void UYanJoystickWidget::StopInputSimulation()
{
	TouchOrigin = FVector2D::ZeroVector;
	StickVector = FVector2D::ZeroVector;
}

#undef LOCTEXT_NAMESPACE

