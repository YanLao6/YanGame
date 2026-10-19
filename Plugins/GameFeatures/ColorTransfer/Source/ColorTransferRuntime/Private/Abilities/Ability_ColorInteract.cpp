#include "Abilities/Ability_ColorInteract.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Interaction/InteractionQuery.h"
#include "Interaction/Tasks/AbilityTask_WaitForInteractableTargets_SingleLineTrace.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Ability_ColorInteract)

UAbility_ColorInteract::UAbility_ColorInteract(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TraceProfile(UCollisionProfile::BlockAllDynamic_ProfileName)
{
}

void UAbility_ColorInteract::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!IsActive() || !IsLocallyControlled())
	{
		return;
	}

	FInteractionQuery Query;
	Query.RequestingAvatar = ActorInfo->AvatarActor;
	Query.RequestingController = GetControllerFromActorInfo();

	UAbilityTask_WaitForInteractableTargets_SingleLineTrace* TraceTask = UAbilityTask_WaitForInteractableTargets_SingleLineTrace::WaitForInteractableTargets_SingleLineTrace(
		this, Query, TraceProfile, MakeTargetLocationInfoFromOwnerActor(), InteractionScanRange, InteractionScanRate);
	TraceTask->InteractableObjectsChanged.AddDynamic(this, &ThisClass::OnInteractableOptionsChanged);
	TraceTask->ReadyForActivation();
}

void UAbility_ColorInteract::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		CurrentOptions.Reset();
		RemovePromptWidget();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAbility_ColorInteract::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);

	if (IsLocallyControlled())
	{
		TriggerInteraction();
	}
}

void UAbility_ColorInteract::OnInteractableOptionsChanged(const TArray<FInteractionOption>& InteractableOptions)
{
	CurrentOptions = InteractableOptions;

	if (CurrentOptions.IsEmpty())
	{
		RemovePromptWidget();
		return;
	}

	UUIExtensionSubsystem* ExtensionSubsystem = GetWorld()->GetSubsystem<UUIExtensionSubsystem>();
	if (!PromptExtensionHandle.IsValid() && ExtensionSubsystem && WidgetClass)
	{
		// 以 PlayerState 为上下文，只挂到本玩家的 HUD
		PromptExtensionHandle = ExtensionSubsystem->RegisterExtensionAsWidgetForContext(WidgetExtensionPointTag, GetOwningActorFromActorInfo(), WidgetClass, /*Priority=*/-1);
	}
}

void UAbility_ColorInteract::RemovePromptWidget()
{
	if (PromptExtensionHandle.IsValid())
	{
		// Unregister 不清空句柄，须手动复位，否则 IsValid 仍为真、下次不会重新注册
		PromptExtensionHandle.Unregister();
		PromptExtensionHandle = FUIExtensionHandle();
	}
}
