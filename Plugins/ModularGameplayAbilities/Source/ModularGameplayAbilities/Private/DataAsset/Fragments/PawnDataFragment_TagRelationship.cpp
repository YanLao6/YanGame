// Copyright Chronicler.

#include "DataAsset/Fragments/PawnDataFragment_TagRelationship.h"

#include "AbilitySystemGlobals.h"
#include "ActorComponent/ModularAbilitySystemComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PawnDataFragment_TagRelationship)

#define LOCTEXT_NAMESPACE "PawnDataFragment_TagRelationship"

namespace PawnDataFragment_TagRelationship
{
	static UModularAbilitySystemComponent* GetModularASC(const FPawnDataFragmentContext& Context)
	{
		return Context.TargetActor
			       ? Cast<UModularAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Context.TargetActor))
			       : nullptr;
	}
}

UPawnDataFragmentState* UPawnDataFragment_TagRelationship::Apply(const FPawnDataFragmentContext& Context) const
{
	if (UModularAbilitySystemComponent* ModularASC = PawnDataFragment_TagRelationship::GetModularASC(Context))
	{
		ModularASC->SetTagRelationshipMapping(TagRelationshipMapping);
	}

	return nullptr;
}

void UPawnDataFragment_TagRelationship::Revoke(const FPawnDataFragmentContext& Context, UPawnDataFragmentState* State) const
{
	if (UModularAbilitySystemComponent* ModularASC = PawnDataFragment_TagRelationship::GetModularASC(Context))
	{
		ModularASC->SetTagRelationshipMapping(nullptr);
	}
}

#if WITH_EDITOR
EDataValidationResult UPawnDataFragment_TagRelationship::IsFragmentValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsFragmentValid(Context);

	if (!TagRelationshipMapping)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(LOCTEXT("NullMapping", "Ability Tag Relationship Fragment 未配置 TagRelationshipMapping。"));
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
