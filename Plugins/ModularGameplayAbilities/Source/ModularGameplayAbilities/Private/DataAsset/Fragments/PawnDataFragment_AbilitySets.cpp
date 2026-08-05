// Copyright Chronicler.

#include "DataAsset/Fragments/PawnDataFragment_AbilitySets.h"

#include "AbilitySystemGlobals.h"
#include "ModularGameplayAbilitiesLogChannels.h"
#include "ActorComponent/ModularAbilitySystemComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PawnDataFragment_AbilitySets)

#define LOCTEXT_NAMESPACE "PawnDataFragment_AbilitySets"

UPawnDataFragmentState* UPawnDataFragment_AbilitySets::Apply(const FPawnDataFragmentContext& Context) const
{
	AActor* Target = Context.TargetActor;
	if (!Target)
	{
		return nullptr;
	}

	UModularAbilitySystemComponent* ModularASC = Cast<UModularAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target));
	if (!ModularASC)
	{
		UE_LOG(LogModularGameplayAbilities, Error, TEXT("Ability Sets Fragment 无法在 [%s] 上找到 ModularAbilitySystemComponent，技能未授予。"), *GetNameSafe(Target));
		return nullptr;
	}

	UPawnDataFragmentState_AbilitySets* State = NewObject<UPawnDataFragmentState_AbilitySets>(Target);
	State->GrantedHandles.Reserve(AbilitySets.Num());

	for (const UModularAbilitySet* AbilitySet : AbilitySets)
	{
		if (!AbilitySet)
		{
			continue;
		}

		AbilitySet->GiveToAbilitySystem(ModularASC, &State->GrantedHandles.AddDefaulted_GetRef());
	}

	return State;
}

void UPawnDataFragment_AbilitySets::Revoke(const FPawnDataFragmentContext& Context, UPawnDataFragmentState* State) const
{
	UPawnDataFragmentState_AbilitySets* TypedState = Cast<UPawnDataFragmentState_AbilitySets>(State);
	if (!TypedState || !Context.TargetActor)
	{
		return;
	}

	UModularAbilitySystemComponent* ModularASC = Cast<UModularAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Context.TargetActor));
	if (!ModularASC)
	{
		return;
	}

	for (FModularAbilitySet_GrantedHandles& Handles : TypedState->GrantedHandles)
	{
		Handles.TakeFromAbilitySystem(ModularASC);
	}

	TypedState->GrantedHandles.Reset();
}

#if WITH_EDITOR
EDataValidationResult UPawnDataFragment_AbilitySets::IsFragmentValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsFragmentValid(Context);

	int32 SetIndex = 0;
	for (const TObjectPtr<UModularAbilitySet>& AbilitySet : AbilitySets)
	{
		if (!AbilitySet)
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("NullAbilitySet", "Ability Sets Fragment 的 AbilitySets[{0}] 为空。"), FText::AsNumber(SetIndex)));
		}

		++SetIndex;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
