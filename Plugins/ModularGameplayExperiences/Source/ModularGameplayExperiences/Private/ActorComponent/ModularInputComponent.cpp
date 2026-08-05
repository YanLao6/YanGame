// Copyright Chronicler.


#include "ActorComponent/ModularInputComponent.h"

#include "CommonLocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "ModularGameplayExperiencesLogs.h"
#include "ModularGameplayTags.h"
#include "ModularPlayerController.h"
#include "ModularPlayerState.h"
#include "ActorComponent/ModularInputConfigComponent.h"
#include "ActorComponent/ModularPawnComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "DataAsset/Fragments/PawnDataFragment_InputConfig.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularInputComponent)

const FName UModularInputComponent::NAME_ActorFeatureName("ModularInput");

namespace ModularInputComponent
{
	static constexpr float LookYawRate   = 300.0f;
	static constexpr float LookPitchRate = 165.0f;
};

UModularInputComponent::UModularInputComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReadyToBindInputs = false;
}

bool UModularInputComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager,
                                                FGameplayTag                    CurrentState,
                                                FGameplayTag                    DesiredState) const
{
	check(Manager);

	APawn* Pawn = GetPawn<APawn>();

	if (!CurrentState.IsValid() && DesiredState == ModularGameplayTags::InitState_Spawned)
	{
		// 有效 Pawn 即可进入 Spawned
		if (Pawn)
		{
			return true;
		}
	}
	else if (CurrentState == ModularGameplayTags::InitState_Spawned && DesiredState == ModularGameplayTags::InitState_DataAvailable)
	{
		// 需要 PlayerState
		if (!GetPlayerState<AModularPlayerState>())
		{
			return false;
		}

		// Authority / Autonomous：等待 Controller 与 PlayerState Owner 配对完成
		if (Pawn->GetLocalRole() != ROLE_SimulatedProxy)
		{
			const AController* Controller = GetController<AController>();

			if (const bool bHasControllerPairedWithPS = (Controller != nullptr)
			                                            && (Controller->PlayerState != nullptr)
			                                            && (Controller->PlayerState->GetOwner() == Controller); !bHasControllerPairedWithPS)
			{
				return false;
			}
		}

		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();

		if (const bool bIsBot = Pawn->IsBotControlled(); bIsLocallyControlled && !bIsBot)
		{
			// 本地真人玩家：需要 InputComponent、ModularPlayerController 与 LocalPlayer
			if (const AModularPlayerController* ModularPC = GetController<AModularPlayerController>();
				!Pawn->InputComponent || !ModularPC || !ModularPC->GetLocalPlayer())
			{
				return false;
			}
		}

		return true;
	}
	else if (CurrentState == ModularGameplayTags::InitState_DataAvailable && DesiredState == ModularGameplayTags::InitState_DataInitialized)
	{
		// 等待 ModularPlayerState 与 UModularPawnComponent 达 DataInitialized
		const AModularPlayerState* ModularPS = GetPlayerState<AModularPlayerState>();

		return ModularPS && Manager->HasFeatureReachedInitState(Pawn, UModularPawnComponent::NAME_ActorFeatureName, ModularGameplayTags::InitState_DataInitialized);
	}
	else if (CurrentState == ModularGameplayTags::InitState_DataInitialized && DesiredState == ModularGameplayTags::InitState_GameplayReady)
	{
		//@todo 可在此加入 AbilitySystem 等 GameplayReady 门槛
		return true;
	}

	return false;
}

void UModularInputComponent::CheckDefaultInitialization()
{
	static const TArray<FGameplayTag> StateChain =
	{
		ModularGameplayTags::InitState_Spawned,
		ModularGameplayTags::InitState_DataAvailable,
		ModularGameplayTags::InitState_DataInitialized,
		ModularGameplayTags::InitState_GameplayReady
	};

	// 自 BeginPlay 设置的 Spawned 起沿链推进至 GameplayReady
	ContinueInitStateChain(StateChain);
}

void UModularInputComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState)
{
	if (CurrentState != ModularGameplayTags::InitState_DataAvailable || DesiredState != ModularGameplayTags::InitState_DataInitialized)
	{
		return;
	}
	const APawn* Pawn = GetPawn<APawn>();
	if (const APlayerState* PlayerState = GetPlayerState<APlayerState>(); !ensure(Pawn && PlayerState))
	{
		return;
	}

	if (!GetController<APlayerController>())
	{
		return;
	}
	if (Pawn->InputComponent != nullptr)
	{
		InitializePlayerInput(Pawn->InputComponent);
	}
}

void UModularInputComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	if (Params.FeatureName == UModularPawnComponent::NAME_ActorFeatureName)
	{
		if (Params.FeatureState == ModularGameplayTags::InitState_DataInitialized)
		{
			// PawnExtension 已达 DataInitialized：尝试推进本组件
			CheckDefaultInitialization();
		}
	}
}

void UModularInputComponent::OnRegister()
{
	Super::OnRegister();

	if (!GetPawn<APawn>())
	{
		UE_LOG(LogModularGameplayExperiences,
		       Error,
		       TEXT("[UModularInputComponent::OnRegister] This component has been added to a blueprint whose base class is not a Pawn. To use this component, it MUST be placed on a Pawn Blueprint."));

#if WITH_EDITOR
		if (GIsEditor)
		{
			static const FText Message        = NSLOCTEXT("ModularInputComponent", "NotOnPawnError", "has been added to a blueprint whose base class is not a Pawn. To use this component, it MUST be placed on a Pawn Blueprint. This will cause a crash if you PIE!");
			static const FName MessageLogName = TEXT("ModularInputComponent");

			FMessageLog(MessageLogName).Error()
			                          ->AddToken(FUObjectToken::Create(this, FText::FromString(GetNameSafe(this))))
			                          ->AddToken(FTextToken::Create(Message));

			FMessageLog(MessageLogName).Open();
		}
#endif
	}
	else
	{
		// 尽早注册 InitState（仅游戏 World）
		RegisterInitStateFeature();
	}
}

void UModularInputComponent::BeginPlay()
{
	Super::BeginPlay();

	// 监听 ModularPawnComponent 的 InitState
	BindOnActorInitStateChanged(UModularPawnComponent::NAME_ActorFeatureName, FGameplayTag(), false);

	// 标记 Spawned 并尝试继续初始化
	ensure(TryToChangeInitState(ModularGameplayTags::InitState_Spawned));
	CheckDefaultInitialization();
}

void UModularInputComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterInitStateFeature();

	Super::EndPlay(EndPlayReason);
}

void UModularInputComponent::InitializePlayerInput(UInputComponent* PlayerInputComponent)
{
	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return;
	}

	const APlayerController* PlayerController = GetController<APlayerController>();
	if (!PlayerController)
	{
		return;
	}

	InputActionMapping(PlayerInputComponent, Pawn, PlayerController);

	if (ensure(!bReadyToBindInputs))
	{
		bReadyToBindInputs = true;
	}

	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(const_cast<APlayerController*>(PlayerController), UModularInputConfigComponent::NAME_BindInputsNow);
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(const_cast<APawn*>(Pawn), UModularInputConfigComponent::NAME_BindInputsNow);
}

void UModularInputComponent::InputActionMapping(UInputComponent* PlayerInputComponent, const APawn* Pawn, const APlayerController* PlayerController)
{
	const ULocalPlayer* LocalPlayer = (PlayerController ? PlayerController->GetLocalPlayer() : nullptr);
	if (!LocalPlayer)
	{
		return;
	}
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();

	Subsystem->ClearAllMappings();

	// IMC 必须在 ClearAllMappings 之后注册，故组件默认映射与 PawnData 映射统一在此处理。
	auto RegisterInputMappings = [Subsystem](const TArray<FInputMappingContextAndPriority>& Mappings)
	{
		for (const auto& [InputMapping, Priority, bRegisterWithSettings] : Mappings)
		{
			// 使用 LoadSynchronous 确保软引用在此帧可用（对齐 Lyra LyraHeroComponent 实现）
			UInputMappingContext* InputMappingContext = InputMapping.LoadSynchronous();
			if (!InputMappingContext || !bRegisterWithSettings)
			{
				continue;
			}

			if (UEnhancedInputUserSettings* Settings = Subsystem->GetUserSettings())
			{
				Settings->RegisterInputMappingContext(InputMappingContext);
			}

			FModifyContextOptions Options             = {};
			Options.bIgnoreAllPressedKeysUntilRelease = false;
			// 将 MappingContext 加入本地玩家的 Enhanced Input 子系统
			Subsystem->AddMappingContext(InputMappingContext, Priority, Options);
		}
	};

	RegisterInputMappings(DefaultInputMappings);

	const UModularPawnComponent* PawnExtComp = UModularPawnComponent::FindModularPawnComponent(Pawn);
	if (!PawnExtComp)
	{
		return;
	}
	const UModularPawnData* PawnData = PawnExtComp->GetPawnData<UModularPawnData>();
	if (!PawnData)
	{
		return;
	}
	const UPawnDataFragment_InputConfig* InputFragment = PawnData->FindFragment<UPawnDataFragment_InputConfig>();
	if (!InputFragment)
	{
		return;
	}

	RegisterInputMappings(InputFragment->InputMappings);

	const UModularInputConfig* InputConfig = InputFragment->InputConfig;
	if (!InputConfig)
	{
		return;
	}
	if (UModularInputConfigComponent* InputConfigComponent = Cast<UModularInputConfigComponent>(PlayerInputComponent);
		ensureMsgf(InputConfigComponent, TEXT("Unexpected Input Component class! The Input Mappings will not be bound to their inputs. Change the input component to UInputConfigComponent or a subclass of it.")))
	{
		// 合并玩家自定义键位映射
		InputConfigComponent->AddInputMappings(InputConfig, Subsystem);

		// PawnData 自带的绑定与 Pawn 同生命周期，无需记录句柄，登记亦无需对称注销。
		InputConfigComponent->RegisterInputConfig(InputConfig);
		BindNativeInputActions(InputConfigComponent, InputConfig, /*OutHandles=*/ nullptr);
	}
}

void UModularInputComponent::BindNativeInputActions(UModularInputConfigComponent* InputConfigComponent, const UModularInputConfig* InputConfig, TArray<uint32>* OutHandles)
{
	auto BindTag = [this, InputConfigComponent, InputConfig, OutHandles](const FGameplayTag& InputTag, auto Func)
	{
		const uint32 Handle = InputConfigComponent->BindNativeAction(InputConfig, InputTag, ETriggerEvent::Triggered, this, Func, /*bLogIfNotFound=*/ false);
		if (OutHandles && Handle != 0)
		{
			OutHandles->Add(Handle);
		}
	};

	BindTag(ModularGameplayTags::InputTag_Move, &ThisClass::Input_Move);
	BindTag(ModularGameplayTags::InputTag_Look_Mouse, &ThisClass::Input_LookMouse);
	BindTag(ModularGameplayTags::InputTag_Look_Stick, &ThisClass::Input_LookStick);
	BindTag(ModularGameplayTags::InputTag_Crouch, &ThisClass::Input_Crouch);
}

void UModularInputComponent::AddAdditionalInputConfig(const UModularInputConfig* InputConfig)
{
	if (!InputConfig)
	{
		return;
	}

	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return;
	}

	UModularInputConfigComponent* InputConfigComponent = Cast<UModularInputConfigComponent>(Pawn->InputComponent);
	if (!ensureMsgf(InputConfigComponent, TEXT("Unexpected Input Component class! The native inputs will not be bound to their actions. Change the input component to UInputConfigComponent or a subclass of it.")))
	{
		return;
	}

	TArray<uint32>& BindHandles = AdditionalInputBindHandles.FindOrAdd(InputConfig);
	if (!BindHandles.IsEmpty())
	{
		// 同一 InputConfig 已绑定，重复绑定会让一次按键触发多次回调。
		return;
	}

	InputConfigComponent->RegisterInputConfig(InputConfig);
	BindNativeInputActions(InputConfigComponent, InputConfig, &BindHandles);
}

void UModularInputComponent::RemoveAdditionalInputConfig(const UModularInputConfig* InputConfig)
{
	if (!InputConfig)
	{
		return;
	}

	TArray<uint32> BindHandles;
	if (!AdditionalInputBindHandles.RemoveAndCopyValue(InputConfig, BindHandles))
	{
		return;
	}

	const APawn* Pawn = GetPawn<APawn>();
	if (UModularInputConfigComponent* InputConfigComponent = Pawn ? Cast<UModularInputConfigComponent>(Pawn->InputComponent) : nullptr)
	{
		InputConfigComponent->RemoveBinds(BindHandles);
		InputConfigComponent->UnregisterInputConfig(InputConfig);
	}
}

void UModularInputComponent::Input_Move(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();

	if (const AController* Controller = Pawn ? Pawn->GetController() : nullptr)
	{
		const FVector2D Value = InputActionValue.Get<FVector2D>();
		const FRotator  MovementRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);

		if (Value.X != 0.0f)
		{
			const FVector MovementDirection = MovementRotation.RotateVector(FVector::RightVector);
			Pawn->AddMovementInput(MovementDirection, Value.X);
		}

		if (Value.Y != 0.0f)
		{
			const FVector MovementDirection = MovementRotation.RotateVector(FVector::ForwardVector);
			Pawn->AddMovementInput(MovementDirection, Value.Y);
		}
	}
}

void UModularInputComponent::Input_LookMouse(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();

	if (!Pawn)
	{
		return;
	}

	const FVector2D Value = InputActionValue.Get<FVector2D>();

	if (Value.X != 0.0f)
	{
		Pawn->AddControllerYawInput(Value.X);
	}

	if (Value.Y != 0.0f)
	{
		Pawn->AddControllerPitchInput(Value.Y);
	}
}

void UModularInputComponent::Input_LookStick(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();

	if (!Pawn)
	{
		return;
	}

	const FVector2D Value = InputActionValue.Get<FVector2D>();

	const UWorld* World = GetWorld();
	check(World);

	if (Value.X != 0.0f)
	{
		Pawn->AddControllerYawInput(Value.X * ModularInputComponent::LookYawRate * World->GetDeltaSeconds());
	}

	if (Value.Y != 0.0f)
	{
		Pawn->AddControllerPitchInput(Value.Y * ModularInputComponent::LookPitchRate * World->GetDeltaSeconds());
	}
}

void UModularInputComponent::Input_Crouch(const FInputActionValue& InputActionValue)
{
	if (ACharacter* Character = GetPawn<ACharacter>())
	{
		if (Character->bIsCrouched)
		{
			Character->Crouch();
		}
		else
		{
			Character->UnCrouch();
		}
	}
}
