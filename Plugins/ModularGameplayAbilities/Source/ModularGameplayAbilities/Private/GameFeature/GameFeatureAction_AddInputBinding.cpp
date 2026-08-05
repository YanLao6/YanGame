// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFeature/GameFeatureAction_AddInputBinding.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "ActorComponent/ModularAbilityExtensionComponent.h"
#include "ActorComponent/ModularInputComponent.h"
#include "ActorComponent/ModularInputConfigComponent.h"
#include "DataAsset/ModularInputConfig.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureAction_AddInputBinding)

#define LOCTEXT_NAMESPACE "GameFeatures"

void UGameFeatureAction_AddInputBinding::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	FPerContextData& ActiveData = ContextData.FindOrAdd(Context);

	if (!ensure(ActiveData.ExtensionRequestHandles.IsEmpty()) ||
	    !ensure(ActiveData.PawnsAddedTo.IsEmpty()))
	{
		Reset(ActiveData);
	}
	Super::OnGameFeatureActivating(Context);
}

void UGameFeatureAction_AddInputBinding::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	if (FPerContextData* ActiveData = ContextData.Find(Context); ensure(ActiveData))
	{
		Reset(*ActiveData);
	}
}

#if WITH_EDITOR
EDataValidationResult UGameFeatureAction_AddInputBinding::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	int32 Index = 0;
	for (const TSoftObjectPtr<const UModularInputConfig>& Entry : InputConfigs)
	{
		if (Entry.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("NullInputConfig", "Null InputConfig at index {0}."), FText::AsNumber(Index)));
		}
		++Index;
	}

	return Result;
}
#endif

void UGameFeatureAction_AddInputBinding::AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
{
	const UWorld*    World        = WorldContext.World();
	UGameInstance*   GameInstance = WorldContext.OwningGameInstance;
	FPerContextData& ActiveData   = ContextData.FindOrAdd(ChangeContext);

	if (!GameInstance || !World || !World->IsGameWorld())
	{
		return;
	}

	if (UGameFrameworkComponentManager* ComponentManager = UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance))
	{
		UGameFrameworkComponentManager::FExtensionHandlerDelegate AddInputBindingDelegate =
			UGameFrameworkComponentManager::FExtensionHandlerDelegate::CreateUObject(this, &ThisClass::HandlePawnExtension, ChangeContext);

		ActiveData.ExtensionRequestHandles.Add(ComponentManager->AddExtensionHandler(APawn::StaticClass(), AddInputBindingDelegate));
	}
}

void UGameFeatureAction_AddInputBinding::Reset(FPerContextData& ActiveData)
{
	ActiveData.ExtensionRequestHandles.Empty();

	while (!ActiveData.PawnsAddedTo.IsEmpty())
	{
		TWeakObjectPtr<APawn> PawnPtr = ActiveData.PawnsAddedTo.Top();
		if (PawnPtr.IsValid())
		{
			RemoveInputBinding(PawnPtr.Get(), ActiveData);
		}
		else
		{
			ActiveData.PawnsAddedTo.Pop();
		}
	}
}

void UGameFeatureAction_AddInputBinding::HandlePawnExtension(AActor* Actor, FName EventName, FGameFeatureStateChangeContext ChangeContext)
{
	APawn*           AsPawn     = CastChecked<APawn>(Actor);
	FPerContextData& ActiveData = ContextData.FindOrAdd(ChangeContext);

	if ((EventName == UGameFrameworkComponentManager::NAME_ExtensionRemoved) || (EventName == UGameFrameworkComponentManager::NAME_ReceiverRemoved))
	{
		RemoveInputBinding(AsPawn, ActiveData);
	}
	else if ((EventName == UGameFrameworkComponentManager::NAME_ExtensionAdded) || (EventName == UModularInputConfigComponent::NAME_BindInputsNow))
	{
		AddInputBindingForPlayer(AsPawn, ActiveData);
	}
}

void UGameFeatureAction_AddInputBinding::AddInputBindingForPlayer(APawn* Pawn, FPerContextData& ActiveData)
{
	if (ActiveData.PawnsAddedTo.Contains(Pawn))
	{
		// 已绑定过。BindInputsNow 会由输入组件与 Ability 扩展组件各发一次，此处保证只生效一次。
		return;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController());
	if (!PlayerController || !PlayerController->GetLocalPlayer())
	{
		// 仅本地玩家需要输入绑定。
		return;
	}

	// InputConfig 含 Ability 与 Native 两类输入，分别由两个组件消费。
	UModularAbilityExtensionComponent* AbilityExtComponent = Pawn->FindComponentByClass<UModularAbilityExtensionComponent>();
	UModularInputComponent*            InputComponent      = Pawn->FindComponentByClass<UModularInputComponent>();

	if (!AbilityExtComponent && !InputComponent)
	{
		// Pawn 不消费任何输入绑定。
		return;
	}

	if ((AbilityExtComponent && !AbilityExtComponent->IsReadyToBindInputs())
	    || (InputComponent && !InputComponent->IsReadyToBindInputs()))
	{
		// 两个组件在同一初始化跳内推进且顺序不定，任一未就绪都需等待其补发的 BindInputsNow。
		return;
	}

	for (const TSoftObjectPtr<const UModularInputConfig>& Entry : InputConfigs)
	{
		const UModularInputConfig* InputConfig = Entry.Get();
		if (!InputConfig)
		{
			continue;
		}

		if (AbilityExtComponent)
		{
			AbilityExtComponent->AddAdditionalInputConfig(InputConfig);
		}

		if (InputComponent)
		{
			InputComponent->AddAdditionalInputConfig(InputConfig);
		}
	}

	ActiveData.PawnsAddedTo.AddUnique(Pawn);
}

void UGameFeatureAction_AddInputBinding::RemoveInputBinding(APawn* Pawn, FPerContextData& ActiveData)
{
	UModularAbilityExtensionComponent* AbilityExtComponent = Pawn->FindComponentByClass<UModularAbilityExtensionComponent>();
	UModularInputComponent*            InputComponent      = Pawn->FindComponentByClass<UModularInputComponent>();

	for (const TSoftObjectPtr<const UModularInputConfig>& Entry : InputConfigs)
	{
		const UModularInputConfig* InputConfig = Entry.Get();
		if (!InputConfig)
		{
			continue;
		}

		if (AbilityExtComponent)
		{
			AbilityExtComponent->RemoveAdditionalInputConfig(InputConfig);
		}

		if (InputComponent)
		{
			InputComponent->RemoveAdditionalInputConfig(InputConfig);
		}
	}

	ActiveData.PawnsAddedTo.Remove(Pawn);
}

#undef LOCTEXT_NAMESPACE
