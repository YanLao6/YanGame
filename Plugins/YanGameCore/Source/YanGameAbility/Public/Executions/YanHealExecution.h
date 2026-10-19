#pragma once

#include "GameplayEffectExecutionCalculation.h"

#include "YanHealExecution.generated.h"

#define UE_API YANGAMEABILITY_API

/**
 * 治疗结算。
 * 捕获来源侧 BaseHeal，写入目标的 Healing meta 属性，由生命属性集换算为 +Health。
 */
UCLASS(MinimalAPI)
class UYanHealExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UE_API UYanHealExecution();

protected:
	//~Begin UGameplayEffectExecutionCalculation Interface
	UE_API virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
	//~End UGameplayEffectExecutionCalculation Interface
};

#undef UE_API
