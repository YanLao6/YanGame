#pragma once

#include "GameplayEffectExecutionCalculation.h"

#include "YanDamageExecution.generated.h"

#define UE_API YANGAMEABILITY_API

/**
 * 伤害结算。
 * 捕获来源侧 BaseDamage，依次施加队伍规则、距离衰减与物理材质衰减，
 * 最终写入目标的 Damage meta 属性，由生命属性集换算为 -Health。
 */
UCLASS(MinimalAPI)
class UYanDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UE_API UYanDamageExecution();

protected:
	//~Begin UGameplayEffectExecutionCalculation Interface
	UE_API virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
	//~End UGameplayEffectExecutionCalculation Interface
};

#undef UE_API
