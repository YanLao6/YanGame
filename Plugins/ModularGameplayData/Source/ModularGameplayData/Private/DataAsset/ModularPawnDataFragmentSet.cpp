// Copyright Chronicler.

#include "DataAsset/ModularPawnDataFragmentSet.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularPawnDataFragmentSet)

#define LOCTEXT_NAMESPACE "ModularPawnDataFragmentSet"

#if WITH_EDITOR
EDataValidationResult UModularPawnDataFragmentSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	int32 FragmentIndex = 0;
	for (const TObjectPtr<UPawnDataFragment>& Fragment : Fragments)
	{
		if (!Fragment)
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("NullFragment", "Fragments[{0}] 为空。"), FText::AsNumber(FragmentIndex)));
		}
		else
		{
			Result = CombineDataValidationResults(Result, Fragment->IsFragmentValid(Context));
		}

		++FragmentIndex;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
