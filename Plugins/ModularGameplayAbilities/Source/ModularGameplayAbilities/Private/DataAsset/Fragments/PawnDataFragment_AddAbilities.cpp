// Copyright Chronicler.

#include "DataAsset/Fragments/PawnDataFragment_AddAbilities.h"

#include "AbilitySystemGlobals.h"
#include "ModularGameplayAbilitiesLogChannels.h"
#include "ActorComponent/ModularAbilitySystemComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PawnDataFragment_AddAbilities)

#define LOCTEXT_NAMESPACE "PawnDataFragment_AddAbilities"

UPawnDataFragmentState* UPawnDataFragment_AddAbilities::Apply(const FPawnDataFragmentContext& Context) const
{
	AActor* Target = Context.TargetActor;
	if (!Target)
	{
		return nullptr;
	}

	UModularAbilitySystemComponent* ModularASC = Cast<UModularAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target));
	if (!ModularASC)
	{
		UE_LOG(LogModularGameplayAbilities, Error, TEXT("Add Abilities Fragment 无法在 [%s] 上找到 ModularAbilitySystemComponent，技能未授予。"), *GetNameSafe(Target));
		return nullptr;
	}

	UPawnDataFragmentState_AddAbilities* State = NewObject<UPawnDataFragmentState_AddAbilities>(Target);
	ModularAbilityGrant::GiveToAbilitySystem(ModularASC, GrantedAbilities, GrantedEffects, GrantedAttributes, GrantedAbilitySets, State->GrantedHandles);
	return State;
}

void UPawnDataFragment_AddAbilities::Revoke(const FPawnDataFragmentContext& Context, UPawnDataFragmentState* State) const
{
	UPawnDataFragmentState_AddAbilities* TypedState = Cast<UPawnDataFragmentState_AddAbilities>(State);
	if (!TypedState || !Context.TargetActor)
	{
		return;
	}

	UModularAbilitySystemComponent* ModularASC = Cast<UModularAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Context.TargetActor));
	if (!ModularASC)
	{
		return;
	}

	TypedState->GrantedHandles.TakeFromAbilitySystem(ModularASC);
}

#if WITH_EDITOR
EDataValidationResult UPawnDataFragment_AddAbilities::IsFragmentValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsFragmentValid(Context);

	if (GrantedAbilities.IsEmpty() && GrantedAttributes.IsEmpty() && GrantedEffects.IsEmpty() && GrantedAbilitySets.IsEmpty())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(LOCTEXT("NoAddOns", "Add Abilities Fragment 未配置任何 Ability、AttributeSet、GameplayEffect 或 AbilitySet。"));
	}

	for (int32 Index = 0; Index < GrantedAbilities.Num(); ++Index)
	{
		if (GrantedAbilities[Index].AbilityType.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("NullAbility", "Add Abilities Fragment 的 GrantedAbilities[{0}] 为空。"), FText::AsNumber(Index)));
		}
	}

	for (int32 Index = 0; Index < GrantedAttributes.Num(); ++Index)
	{
		if (GrantedAttributes[Index].AttributeSetType.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("NullAttributeSet", "Add Abilities Fragment 的 GrantedAttributes[{0}] 为空。"), FText::AsNumber(Index)));
		}
	}

	for (int32 Index = 0; Index < GrantedEffects.Num(); ++Index)
	{
		if (GrantedEffects[Index].EffectType.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("NullEffect", "Add Abilities Fragment 的 GrantedEffects[{0}] 为空。"), FText::AsNumber(Index)));
		}
	}

	for (int32 Index = 0; Index < GrantedAbilitySets.Num(); ++Index)
	{
		if (GrantedAbilitySets[Index].IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("NullAbilitySet", "Add Abilities Fragment 的 GrantedAbilitySets[{0}] 为空。"), FText::AsNumber(Index)));
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
