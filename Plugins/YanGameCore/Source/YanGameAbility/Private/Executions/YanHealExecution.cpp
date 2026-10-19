#include "Executions/YanHealExecution.h"

#include "AttributeSets/YanBaseCombatSet.h"
#include "AttributeSets/YanHealthSet.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanHealExecution)

namespace
{
	struct FYanHealStatics
	{
		FGameplayEffectAttributeCaptureDefinition BaseHealDef;

		FYanHealStatics()
		{
			BaseHealDef = FGameplayEffectAttributeCaptureDefinition(UYanBaseCombatSet::GetBaseHealAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);
		}
	};

	const FYanHealStatics& HealStatics()
	{
		static FYanHealStatics Statics;
		return Statics;
	}
}

UYanHealExecution::UYanHealExecution()
{
	RelevantAttributesToCapture.Add(HealStatics().BaseHealDef);
}

void UYanHealExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
#if WITH_SERVER_CODE
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	FAggregatorEvaluateParameters EvaluateParameters;
	EvaluateParameters.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvaluateParameters.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	float BaseHeal = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(HealStatics().BaseHealDef, EvaluateParameters, BaseHeal);

	const float HealingDone = FMath::Max(0.0f, BaseHeal);

	if (HealingDone > 0.0f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UYanHealthSet::GetHealingAttribute(), EGameplayModOp::Additive, HealingDone));
	}
#endif // WITH_SERVER_CODE
}
