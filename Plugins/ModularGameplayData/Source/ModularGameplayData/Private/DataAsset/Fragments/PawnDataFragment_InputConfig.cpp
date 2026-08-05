// Copyright Chronicler.

#include "DataAsset/Fragments/PawnDataFragment_InputConfig.h"

#include "DataAsset/ModularInputConfig.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PawnDataFragment_InputConfig)

#define LOCTEXT_NAMESPACE "PawnDataFragment_InputConfig"

#if WITH_EDITOR
EDataValidationResult UPawnDataFragment_InputConfig::IsFragmentValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsFragmentValid(Context);

	if (!InputConfig && InputMappings.IsEmpty() && AdditionalAbilityInputConfigs.IsEmpty())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(LOCTEXT("EmptyInputFragment", "Input Config Fragment 未配置任何 InputConfig 或 InputMappings，不会产生任何效果。"));
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
