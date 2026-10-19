// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/YanSimulatedInputWidget.h"
#include "EnhancedInputSubsystems.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputKeyEventArgs.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanSimulatedInputWidget)

#define LOCTEXT_NAMESPACE "YanSimulatedInputWidget"

UYanSimulatedInputWidget::UYanSimulatedInputWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetConsumePointerInput(true);
}

#if WITH_EDITOR
const FText UYanSimulatedInputWidget::GetPaletteCategory()
{
	return LOCTEXT("PalleteCategory", "Input");
}
#endif 

void UYanSimulatedInputWidget::NativeConstruct()
{
	
	QueryKeyToSimulate();

	if (UEnhancedInputLocalPlayerSubsystem* System = GetEnhancedInputSubsystem())
	{
		System->ControlMappingsRebuiltDelegate.AddUniqueDynamic(this, &UYanSimulatedInputWidget::OnControlMappingsRebuilt);
	}
	
	Super::NativeConstruct();
}

void UYanSimulatedInputWidget::NativeDestruct()
{
	if (UEnhancedInputLocalPlayerSubsystem* System = GetEnhancedInputSubsystem())
	{
		System->ControlMappingsRebuiltDelegate.RemoveAll(this);
	}

	Super::NativeDestruct();
}

FReply UYanSimulatedInputWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	FlushSimulatedInput();
	
	return Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
}

UEnhancedInputLocalPlayerSubsystem* UYanSimulatedInputWidget::GetEnhancedInputSubsystem() const
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (ULocalPlayer* LP = GetOwningLocalPlayer())
		{
			return LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		}
	}
	return nullptr;
}

UEnhancedPlayerInput* UYanSimulatedInputWidget::GetPlayerInput() const
{
	if (UEnhancedInputLocalPlayerSubsystem* System = GetEnhancedInputSubsystem())
	{
		return System->GetPlayerInput();
	}
	return nullptr;
}

void UYanSimulatedInputWidget::InputKeyValue(const FVector& Value)
{
	const APlayerController* PC = GetOwningPlayer();
	const FPlatformUserId UserId = PC ? PC->GetPlatformUserId() : PLATFORMUSERID_NONE;
	
	if (AssociatedAction)
	{
		if (UEnhancedInputLocalPlayerSubsystem* System = GetEnhancedInputSubsystem())
		{
			
			TArray<UInputModifier*> Modifiers;
			TArray<UInputTrigger*> Triggers;
			System->InjectInputVectorForAction(AssociatedAction, Value, Modifiers, Triggers);
		}
	}
	
	else if (UEnhancedPlayerInput* Input = GetPlayerInput())
	{
		const FInputDeviceId DeviceToSimulate = IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(UserId);
		if(KeyToSimulate.IsValid())
		{
			const float DeltaTime = GetWorld()->GetDeltaSeconds();
			auto SimulateKeyPress = [Input, DeltaTime, DeviceToSimulate](const FKey& KeyToSim, const float Value, const EInputEvent Event)
			{
				FInputKeyEventArgs Args = FInputKeyEventArgs::CreateSimulated(
					KeyToSim,
					Event,
					Value,
					KeyToSim.IsAnalog() ? 1 : 0,
					DeviceToSimulate);

				Args.DeltaTime = DeltaTime;
			
				Input->InputKey(Args);
			};

			

			
			
			if (const EKeys::FPairedKeyDetails* PairDetails = EKeys::GetPairedKeyDetails(KeyToSimulate))
			{
				SimulateKeyPress(PairDetails->XKeyDetails->GetKey(), Value.X, IE_Axis);
				SimulateKeyPress(PairDetails->YKeyDetails->GetKey(), Value.Y, IE_Axis);
			}
			else
			{
				SimulateKeyPress(KeyToSimulate, Value.X, IE_Pressed);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("'%s' is attempting to simulate input but has no player input!"), *GetNameSafe(this));
	}
}

void UYanSimulatedInputWidget::InputKeyValue2D(const FVector2D& Value)
{
	InputKeyValue(FVector(Value.X, Value.Y, 0.0));
}

void UYanSimulatedInputWidget::FlushSimulatedInput()
{
	if (UEnhancedPlayerInput* Input = GetPlayerInput())
	{
		Input->FlushPressedKeys();
	}
}

void UYanSimulatedInputWidget::QueryKeyToSimulate()
{
	if (UEnhancedInputLocalPlayerSubsystem* System = GetEnhancedInputSubsystem())
	{
		TArray<FKey> Keys = System->QueryKeysMappedToAction(AssociatedAction);
		if(!Keys.IsEmpty() && Keys[0].IsValid())
		{
			KeyToSimulate = Keys[0];
		}
		else
		{
			KeyToSimulate = FallbackBindingKey;
		}
	}
}

void UYanSimulatedInputWidget::OnControlMappingsRebuilt()
{
	QueryKeyToSimulate();
}

#undef LOCTEXT_NAMESPACE
