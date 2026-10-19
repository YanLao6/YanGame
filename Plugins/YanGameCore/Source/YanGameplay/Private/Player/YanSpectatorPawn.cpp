// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/YanSpectatorPawn.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawnMovement.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Player/YanPlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanSpectatorPawn)

AYanSpectatorPawn::AYanSpectatorPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
}

void AYanSpectatorPawn::PawnClientRestart()
{
	Super::PawnClientRestart();

	ApplySpectateInputMappings(/*bApply=*/true);
	RegisterSpectateWidget();
}

void AYanSpectatorPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterSpectateWidget();
	ApplySpectateInputMappings(/*bApply=*/false);

	Super::EndPlay(EndPlayReason);
}

void AYanSpectatorPawn::RegisterSpectateWidget()
{
	if (SpectateWidgetClass == nullptr || !SpectateWidgetSlot.IsValid() || SpectateWidgetHandle.IsValid())
	{
		return;
	}

	const APlayerController* OwningPlayerController = Cast<APlayerController>(GetController());
	ULocalPlayer* LocalPlayer = OwningPlayerController != nullptr ? OwningPlayerController->GetLocalPlayer() : nullptr;
	if (LocalPlayer == nullptr)
	{
		return;
	}

	UUIExtensionSubsystem* ExtensionSubsystem = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UUIExtensionSubsystem>() : nullptr;
	if (ExtensionSubsystem == nullptr)
	{
		return;
	}

	SpectateWidgetHandle = ExtensionSubsystem->RegisterExtensionAsWidgetForContext(SpectateWidgetSlot, LocalPlayer, SpectateWidgetClass, /*Priority=*/-1);
}

void AYanSpectatorPawn::UnregisterSpectateWidget()
{
	if (SpectateWidgetHandle.IsValid())
	{
		SpectateWidgetHandle.Unregister();
	}
}

void AYanSpectatorPawn::ApplySpectateInputMappings(bool bApply)
{
	const APlayerController* OwningPlayerController = Cast<APlayerController>(GetController());
	const ULocalPlayer* LocalPlayer = OwningPlayerController != nullptr ? OwningPlayerController->GetLocalPlayer() : nullptr;
	if (LocalPlayer == nullptr)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (InputSubsystem == nullptr)
	{
		return;
	}

	for (const TObjectPtr<UInputMappingContext>& Mapping : SpectateInputMappings)
	{
		if (Mapping == nullptr)
		{
			continue;
		}

		if (bApply)
		{
			InputSubsystem->AddMappingContext(Mapping, SpectateInputPriority);
		}
		else
		{
			InputSubsystem->RemoveMappingContext(Mapping);
		}
	}
}

void AYanSpectatorPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// 有意跳过 ADefaultPawn 的旧式 axis 绑定，观战输入统一走 EnhancedInput
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!ensureMsgf(EnhancedInputComponent, TEXT("观战 Pawn 需要 UEnhancedInputComponent，请检查 DefaultInputComponentClass 配置。")))
	{
		return;
	}

	if (MoveAction)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
	}
	if (LookAction)
	{
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
	}
	if (MoveVerticalAction)
	{
		EnhancedInputComponent->BindAction(MoveVerticalAction, ETriggerEvent::Triggered, this, &ThisClass::Input_MoveVertical);
	}
	if (SprintAction)
	{
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &ThisClass::Input_SprintStart);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &ThisClass::Input_SprintStop);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Canceled, this, &ThisClass::Input_SprintStop);
	}
	if (NextTargetAction)
	{
		EnhancedInputComponent->BindAction(NextTargetAction, ETriggerEvent::Started, this, &ThisClass::Input_NextTarget);
	}
	if (PreviousTargetAction)
	{
		EnhancedInputComponent->BindAction(PreviousTargetAction, ETriggerEvent::Started, this, &ThisClass::Input_PreviousTarget);
	}
	if (ToggleFreeCameraAction)
	{
		EnhancedInputComponent->BindAction(ToggleFreeCameraAction, ETriggerEvent::Started, this, &ThisClass::Input_ToggleFreeCamera);
	}
	if (LeaveSpectateAction)
	{
		EnhancedInputComponent->BindAction(LeaveSpectateAction, ETriggerEvent::Started, this, &ThisClass::Input_LeaveSpectate);
	}
}

void AYanSpectatorPawn::Input_Move(const FInputActionValue& InputActionValue)
{
	const FVector2D MoveValue = InputActionValue.Get<FVector2D>();

	MoveRight(MoveValue.X);
	MoveForward(MoveValue.Y);
}

void AYanSpectatorPawn::Input_Look(const FInputActionValue& InputActionValue)
{
	const FVector2D LookValue = InputActionValue.Get<FVector2D>();

	AddControllerYawInput(LookValue.X);
	AddControllerPitchInput(LookValue.Y);
}

void AYanSpectatorPawn::Input_MoveVertical(const FInputActionValue& InputActionValue)
{
	MoveUp_World(InputActionValue.Get<float>());
}

void AYanSpectatorPawn::Input_SprintStart()
{
	USpectatorPawnMovement* SpectatorMovement = Cast<USpectatorPawnMovement>(GetMovementComponent());
	if (SpectatorMovement == nullptr)
	{
		return;
	}

	BaseMaxSpeed = SpectatorMovement->MaxSpeed;
	SpectatorMovement->MaxSpeed = BaseMaxSpeed * SprintSpeedMultiplier;
}

void AYanSpectatorPawn::Input_SprintStop()
{
	USpectatorPawnMovement* SpectatorMovement = Cast<USpectatorPawnMovement>(GetMovementComponent());
	if (SpectatorMovement == nullptr || BaseMaxSpeed <= 0.0f)
	{
		return;
	}

	SpectatorMovement->MaxSpeed = BaseMaxSpeed;
	BaseMaxSpeed = 0.0f;
}

void AYanSpectatorPawn::Input_NextTarget()
{
	if (AYanPlayerController* YanPlayerController = GetYanPlayerController())
	{
		YanPlayerController->SpectateNextPlayer();
	}
}

void AYanSpectatorPawn::Input_PreviousTarget()
{
	if (AYanPlayerController* YanPlayerController = GetYanPlayerController())
	{
		YanPlayerController->SpectatePreviousPlayer();
	}
}

void AYanSpectatorPawn::Input_ToggleFreeCamera()
{
	AYanPlayerController* YanPlayerController = GetYanPlayerController();
	if (YanPlayerController == nullptr)
	{
		return;
	}

	if (YanPlayerController->GetSpectateTargetPlayerState() != nullptr)
	{
		YanPlayerController->SpectateFreeCamera();
	}
	else
	{
		YanPlayerController->SpectateNextPlayer();
	}
}

void AYanSpectatorPawn::Input_LeaveSpectate()
{
	if (AYanPlayerController* YanPlayerController = GetYanPlayerController())
	{
		YanPlayerController->LeaveSpectate();
	}
}

AYanPlayerController* AYanSpectatorPawn::GetYanPlayerController() const
{
	return Cast<AYanPlayerController>(GetController());
}
