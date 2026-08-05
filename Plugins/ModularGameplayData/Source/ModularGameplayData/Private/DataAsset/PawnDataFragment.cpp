// Copyright Chronicler.

#include "DataAsset/PawnDataFragment.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PawnDataFragment)

#if WITH_EDITOR
EDataValidationResult UPawnDataFragment::IsFragmentValid(FDataValidationContext& Context) const
{
	return EDataValidationResult::Valid;
}
#endif
