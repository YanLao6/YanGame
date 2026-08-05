// Copyright Chronicler.

#include "ActorComponent/ModularAbilityExtensionComponent.h"

#include "AbilitySystemInterface.h"
#include "ModularAbilityTags.h"
#include "ModularGameplayAbilitiesLogChannels.h"
#include "ModularGameplayTags.h"
#include "ModularPlayerState.h"
#include "Actor/ModularExperienceCharacter.h"
#include "Actor/ModularExperiencePawn.h"
#include "ActorComponent/ModularAbilitySystemComponent.h"
#include "ActorComponent/ModularInputComponent.h"
#include "ActorComponent/ModularInputConfigComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "DataAsset/Fragments/PawnDataFragment_InputConfig.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularAbilityExtensionComponent)

const FName UModularAbilityExtensionComponent::NAME_ActorFeatureName("ModularAbilityExtension");

UModularAbilityExtensionComponent::UModularAbilityExtensionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick          = false;

	SetIsReplicatedByDefault(true);

	AbilitySystemComponent = nullptr;
}

void UModularAbilityExtensionComponent::InitializeAbilitySystem(UModularAbilitySystemComponent* InASC, AActor* InOwnerActor)
{
	check(InASC);
	check(InOwnerActor);

	if (AbilitySystemComponent == InASC)
	{
		// ASC 未变化，无需重复初始化。
		return;
	}

	if (AbilitySystemComponent)
	{
		// 切换 ASC 前先清理旧的绑定关系。
		UninitializeAbilitySystem();
	}

	AModularExperiencePawn* Pawn           = GetPawnChecked<AModularExperiencePawn>();
	const AActor*           ExistingAvatar = InASC->GetAvatarActor();

	UE_LOG(LogModularGameplayAbilities, Verbose, TEXT("Setting up ASC [%s] on pawn [%s] owner [%s], existing [%s] "), *GetNameSafe(InASC), *GetNameSafe(Pawn), *GetNameSafe(InOwnerActor), *GetNameSafe(ExistingAvatar));

	if ((ExistingAvatar != nullptr) && (ExistingAvatar != Pawn))
	{
		UE_LOG(LogModularGameplayAbilities, Log, TEXT("Existing avatar (authority=%d)"), ExistingAvatar->HasAuthority() ? 1 : 0);

		// ASC 上已有其他 Pawn 作为 Avatar：需要先踢掉旧 Avatar（常见于 Client 延迟导致新 Pawn 先 Possess）。
		ensure(!ExistingAvatar->HasAuthority());

		if (UModularAbilityExtensionComponent* OtherExtensionComponent = ExistingAvatar->FindComponentByClass<UModularAbilityExtensionComponent>())
		{
			OtherExtensionComponent->UninitializeAbilitySystem();
		}
	}

	AbilitySystemComponent = InASC;
	AbilitySystemComponent->InitAbilityActorInfo(InOwnerActor, Pawn);

	// TagRelationshipMapping 由 PawnData 的 PlayerState 作用域 Fragment 写入 ASC，此处不再重复设置。

	OnAbilitySystemInitialized.Broadcast();
}

void UModularAbilityExtensionComponent::UninitializeAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// 若本 Actor 仍是 Avatar，才执行 ASC 清理（否则可能已被新 Pawn 接管）。
	if (AbilitySystemComponent->GetAvatarActor() == GetOwner())
	{
		FGameplayTagContainer AbilityTypesToIgnore;
		AbilityTypesToIgnore.AddTag(ModularAbilityTags::Ability_Behavior_SurvivesDeath);

		AbilitySystemComponent->CancelAbilities(nullptr, &AbilityTypesToIgnore);
		AbilitySystemComponent->ClearAbilityInput();
		AbilitySystemComponent->RemoveAllGameplayCues();

		if (AbilitySystemComponent->GetOwnerActor() != nullptr)
		{
			AbilitySystemComponent->SetAvatarActor(nullptr);
		}
		else
		{
			// Owner 无效时不能只清 Avatar，需要完整 ClearActorInfo。
			AbilitySystemComponent->ClearActorInfo();
		}

		OnAbilitySystemUninitialized.Broadcast();
	}

	AbilitySystemComponent = nullptr;
}

void UModularAbilityExtensionComponent::HandleControllerChanged()
{
	if (AbilitySystemComponent && (AbilitySystemComponent->GetAvatarActor() == GetPawnChecked<APawn>()))
	{
		ensure(AbilitySystemComponent->AbilityActorInfo->OwnerActor == AbilitySystemComponent->GetOwnerActor());
		if (AbilitySystemComponent->GetOwnerActor() == nullptr)
		{
			UninitializeAbilitySystem();
		}
		else
		{
			AbilitySystemComponent->RefreshAbilityActorInfo();
		}
	}

	CheckDefaultInitialization();
}

void UModularAbilityExtensionComponent::InitializePlayerInput(UInputComponent* PlayerInputComponent)
{
	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return;
	}

	if (const APlayerController* PlayerController = GetController<APlayerController>(); !PlayerController)
	{
		return;
	}

	InputActionMapping(PlayerInputComponent, Pawn);

	bReadyToBindInputs = true;

	// 本组件与 UModularInputComponent 同在 DataAvailable→DataInitialized 一跳内初始化，推进顺序不固定。
	// 此处补发一次事件，确保监听方（GameFeatureAction_AddInputBinding）至少收到一次 IsReadyToBindInputs 为真的通知。
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(const_cast<APawn*>(Pawn), UModularInputConfigComponent::NAME_BindInputsNow);
}

void UModularAbilityExtensionComponent::AddAdditionalInputConfig(const UModularInputConfig* InputConfig)
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
	if (!ensureMsgf(InputConfigComponent, TEXT("Unexpected Input Component class! The Gameplay Abilities will not be bound to their inputs. Change the input component to UInputConfigComponent or a subclass of it.")))
	{
		return;
	}

	TArray<uint32>& BindHandles = AdditionalInputBindHandles.FindOrAdd(InputConfig);
	if (!BindHandles.IsEmpty())
	{
		// 同一 InputConfig 已绑定，重复绑定会让一次按键触发多次技能输入。
		return;
	}

	InputConfigComponent->RegisterInputConfig(InputConfig);
	InputConfigComponent->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, /*out*/ BindHandles);
}

void UModularAbilityExtensionComponent::RemoveAdditionalInputConfig(const UModularInputConfig* InputConfig)
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

void UModularAbilityExtensionComponent::InputActionMapping(UInputComponent* PlayerInputComponent, const APawn* Pawn)
{
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

	UModularInputConfigComponent* InputConfigComponent = Cast<UModularInputConfigComponent>(PlayerInputComponent);
	if (!ensureMsgf(InputConfigComponent, TEXT("Unexpected Input Component class! The Gameplay Abilities will not be bound to their inputs. Change the input component to UInputConfigComponent or a subclass of it.")))
	{
		return;
	}

	if (InputFragment->InputConfig)
	{
		// 将 InputAction 绑定到 GameplayTag，从而让 GameplayAbility Blueprint 能由 Trigger 事件直接驱动。
		TArray<uint32> BindHandles;
		InputConfigComponent->RegisterInputConfig(InputFragment->InputConfig);
		InputConfigComponent->BindAbilityActions(InputFragment->InputConfig, this, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, /*out*/ BindHandles);
	}

	for (const UModularInputConfig* AdditionalConfig : InputFragment->AdditionalAbilityInputConfigs)
	{
		AddAdditionalInputConfig(AdditionalConfig);
	}
}

void UModularAbilityExtensionComponent::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (const APawn* Pawn = GetPawn<APawn>(); !Pawn)
	{
		return;
	}
	if (UModularAbilitySystemComponent* ModularASC = GetModularAbilitySystemComponent())
	{
		ModularASC->AbilityInputTagPressed(InputTag);
	}
}

void UModularAbilityExtensionComponent::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (const APawn* Pawn = GetPawn<APawn>();
		!Pawn)
	{
		return;
	}
	if (UModularAbilitySystemComponent* ModularASC = GetModularAbilitySystemComponent())
	{
		ModularASC->AbilityInputTagReleased(InputTag);
	}
}

// — IGameFrameworkInitStateInterface —

bool UModularAbilityExtensionComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager,
                                                           FGameplayTag                    CurrentState,
                                                           FGameplayTag                    DesiredState) const
{
	check(Manager);

	AModularExperiencePawn* Pawn = GetPawnChecked<AModularExperiencePawn>();
	if (!CurrentState.IsValid()
	    && DesiredState == ModularGameplayTags::InitState_Spawned)
	{
		// 只要挂在有效 Pawn 上，即视为 Spawned。
		if (Pawn)
		{
			return true;
		}
	}
	if (CurrentState == ModularGameplayTags::InitState_Spawned
	    && DesiredState == ModularGameplayTags::InitState_DataAvailable)
	{
		// 需要 PlayerState。
		if (!GetPlayerState<AModularPlayerState>())
		{
			return false;
		}

		// Authority / Autonomous：等待 Controller 与 PlayerState 建立正确归属。
		if (Pawn->GetLocalRole() != ROLE_SimulatedProxy)
		{
			AController* Controller = GetController<AController>();

			const bool bHasControllerPairedWithPS = (Controller != nullptr) &&
			                                        (Controller->PlayerState != nullptr) &&
			                                        (Controller->PlayerState->GetOwner() == Controller);

			if (!bHasControllerPairedWithPS)
			{
				return false;
			}
		}

		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();
		const bool bIsBot               = Pawn->IsBotControlled();

		if (bIsLocallyControlled && !bIsBot)
		{
			AModularPlayerController* ModularPC = GetController<AModularPlayerController>();

			// 本地玩家需要 InputComponent 与 LocalPlayer 就绪。
			if (!Pawn->InputComponent || !ModularPC || !ModularPC->GetLocalPlayer())
			{
				return false;
			}
		}

		return true;
	}
	else if (CurrentState == ModularGameplayTags::InitState_DataAvailable
	         && DesiredState == ModularGameplayTags::InitState_DataInitialized)
	{
		// 等待 PlayerState 上的 ASC 以及 Pawn 核心扩展完成 DataInitialized。
		const IAbilitySystemInterface* ModularPS = Cast<IAbilitySystemInterface>(GetPlayerState<AModularPlayerState>());

		return ModularPS->GetAbilitySystemComponent() && Manager->HasFeatureReachedInitState(Pawn, UModularPawnComponent::NAME_ActorFeatureName, ModularGameplayTags::InitState_DataInitialized);
	}
	else if (CurrentState == ModularGameplayTags::InitState_DataInitialized
	         && DesiredState == ModularGameplayTags::InitState_GameplayReady)
	{
		// @todo 可在此补充 Ability 初始化相关校验。
		return true;
	}

	return false;
}

void UModularAbilityExtensionComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager,
                                                              const FGameplayTag              CurrentState,
                                                              const FGameplayTag              DesiredState)
{
	if (CurrentState == ModularGameplayTags::InitState_DataAvailable && DesiredState == ModularGameplayTags::InitState_DataInitialized)
	{
		const APawn*                   Pawn             = GetPawn<APawn>();
		AModularPlayerState*           ModularPS        = GetPlayerState<AModularPlayerState>();
		const IAbilitySystemInterface* ModularAbilityPS = Cast<IAbilitySystemInterface>(ModularPS);
		UAbilitySystemComponent*       ModularASC       = ModularAbilityPS->GetAbilitySystemComponent();
		if (!ensure(Pawn && ModularPS && ModularASC))
		{
			return;
		}
		// PlayerState 持有跨死亡 persistent 的数据；ASC / AttributeSet 常驻 PlayerState。
		InitializeAbilitySystem(Cast<UModularAbilitySystemComponent>(ModularASC), ModularPS);
		if (Pawn->InputComponent != nullptr)
		{
			InitializePlayerInput(Pawn->InputComponent);
		}
	}
}

void UModularAbilityExtensionComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	if (Params.FeatureName == UModularPawnComponent::NAME_ActorFeatureName)
	{
		if (Params.FeatureState == ModularGameplayTags::InitState_DataAvailable)
		{
			// 其他组件就绪后尝试推进初始化链。
			CheckDefaultInitialization();
		}
	}
}

void UModularAbilityExtensionComponent::CheckDefaultInitialization()
{
	static const TArray<FGameplayTag> StateChain =
	{
		ModularGameplayTags::InitState_Spawned,
		ModularGameplayTags::InitState_DataAvailable,
		ModularGameplayTags::InitState_DataInitialized,
		ModularGameplayTags::InitState_GameplayReady
	};

	// 自 Spawned 起沿链推进，直至 GameplayReady（BeginPlay 中先设 Spawned）。
	ContinueInitStateChain(StateChain);
}

void UModularAbilityExtensionComponent::OnRegister()
{
	Super::OnRegister();

	if (!GetPawn<APawn>())
	{
		UE_LOG(LogModularGameplayAbilities,
		       Error,
		       TEXT("[UModularAbilityExtensionComponent::OnRegister] This component has been added to a blueprint whose base class is not a Pawn. To use this component, it MUST be placed on a Pawn Blueprint."));

#if WITH_EDITOR
		if (GIsEditor)
		{
			static const FText Message        = NSLOCTEXT("ModularAbilityExtensionComponent", "NotOnPawnError", "has been added to a blueprint whose base class is not a Pawn. To use this component, it MUST be placed on a Pawn Blueprint. This will cause a crash if you PIE!");
			static const FName MessageLogName = TEXT("ModularAbilityExtensionComponent");

			FMessageLog(MessageLogName).Error()
			                          ->AddToken(FUObjectToken::Create(this, FText::FromString(GetNameSafe(this))))
			                          ->AddToken(FTextToken::Create(Message));

			FMessageLog(MessageLogName).Open();
		}
#endif
	}
	else
	{
		// 尽早注册到 InitState（仅 GameWorld 有效）。
		RegisterInitStateFeature();
	}
}

void UModularAbilityExtensionComponent::BeginPlay()
{
	Super::BeginPlay();

	// 监听 Pawn 扩展组件的 InitState 变更。
	BindOnActorInitStateChanged(UModularPawnComponent::NAME_ActorFeatureName, FGameplayTag(), false);

	ensure(TryToChangeInitState(ModularGameplayTags::InitState_Spawned));
	CheckDefaultInitialization();
}

void UModularAbilityExtensionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterInitStateFeature();

	Super::EndPlay(EndPlayReason);
}
