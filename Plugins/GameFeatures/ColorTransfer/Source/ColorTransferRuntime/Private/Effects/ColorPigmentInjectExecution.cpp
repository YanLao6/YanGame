#include "Effects/ColorPigmentInjectExecution.h"

#include "Attributes/ColorPigmentSet.h"
#include "Effects/ColorPigmentEffects.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorPigmentInjectExecution)

void UColorPigmentInjectExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	for (const EColorPigment Pigment : {EColorPigment::Red, EColorPigment::Blue, EColorPigment::Black, EColorPigment::White})
	{
		const float Amount = Spec.GetSetByCallerMagnitude(ColorTransfer::GetPigmentSetByCallerTag(Pigment), /*WarnIfNotFound=*/false);
		if (Amount > 0.0f)
		{
			OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UColorPigmentSet::GetInjectAttribute(Pigment), EGameplayModOp::Additive, Amount));
		}
	}
}
