// Copyright Chronicler.

#include "DataAsset/Fragments/PawnDataFragment_AddComponents.h"

#include "ModularGameplayDataLogs.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PawnDataFragment_AddComponents)

#define LOCTEXT_NAMESPACE "PawnDataFragment_AddComponents"

UPawnDataFragmentState* UPawnDataFragment_AddComponents::Apply(const FPawnDataFragmentContext& Context) const
{
	AActor* Target = Context.TargetActor;
	if (!Target)
	{
		return nullptr;
	}

	const bool bHasAuthority = Target->HasAuthority();

	UPawnDataFragmentState_AddComponents* State = nullptr;

	for (const FPawnDataComponentEntry& Entry : Components)
	{
		if (bHasAuthority ? !Entry.bServerComponent : !Entry.bClientComponent)
		{
			continue;
		}

		UClass* ComponentClass = Entry.ComponentClass.LoadSynchronous();
		if (!ComponentClass)
		{
			UE_LOG(LogModularGameplayData, Error, TEXT("Add Components Fragment 上的组件类无法加载，目标 [%s]。"), *GetNameSafe(Target));
			continue;
		}

		UActorComponent* NewComponent = NewObject<UActorComponent>(Target, ComponentClass);
		if (!NewComponent)
		{
			continue;
		}

		NewComponent->RegisterComponent();

		if (!State)
		{
			State = NewObject<UPawnDataFragmentState_AddComponents>(Target);
		}
		State->AddedComponents.Add(NewComponent);
	}

	return State;
}

void UPawnDataFragment_AddComponents::Revoke(const FPawnDataFragmentContext& Context, UPawnDataFragmentState* State) const
{
	UPawnDataFragmentState_AddComponents* TypedState = Cast<UPawnDataFragmentState_AddComponents>(State);
	if (!TypedState)
	{
		return;
	}

	for (const TWeakObjectPtr<UActorComponent>& ComponentPtr : TypedState->AddedComponents)
	{
		if (UActorComponent* Component = ComponentPtr.Get())
		{
			Component->DestroyComponent();
		}
	}

	TypedState->AddedComponents.Reset();
}

#if WITH_EDITOR
EDataValidationResult UPawnDataFragment_AddComponents::IsFragmentValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsFragmentValid(Context);

	int32 EntryIndex = 0;
	for (const FPawnDataComponentEntry& Entry : Components)
	{
		if (Entry.ComponentClass.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("NullComponentClass", "Add Components Fragment 的 Components[{0}] 未指定组件类。"), FText::AsNumber(EntryIndex)));
		}
		else if (!Entry.bServerComponent && !Entry.bClientComponent)
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("NoSideComponent", "Add Components Fragment 的 Components[{0}] 两端均未启用，不会被创建。"), FText::AsNumber(EntryIndex)));
		}

		++EntryIndex;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
