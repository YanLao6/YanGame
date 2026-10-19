#include "GE/MoverLaunchGameplayEffect.h"
#include "GE/YanBreakWindowEffect.h"

#include "GE/MoverLaunchExecutionCalculation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverLaunchGameplayEffect)

UMoverLaunchGameplayEffect::UMoverLaunchGameplayEffect()
{
	// 击飞是一次性事件，不在目标身上驻留
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Execution 在此接死：本类存在的意义正是免去各技能各配一份内容相同的 GE 资产
	FGameplayEffectExecutionDefinition ExecutionDefinition;
	ExecutionDefinition.CalculationClass = UMoverLaunchExecutionCalculation::StaticClass();
	Executions.Add(ExecutionDefinition);

	// 默认给反制机会：不给退路的击飞是特例，须在子类里显式清空本项
	BreakWindowEffect = UYanBreakWindowEffect::StaticClass();
}
