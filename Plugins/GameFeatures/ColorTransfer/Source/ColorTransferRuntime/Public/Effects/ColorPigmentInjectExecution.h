#pragma once

#include "GameplayEffectExecutionCalculation.h"

#include "ColorPigmentInjectExecution.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

/**
 * 颜料注入结算。
 *
 * 颜色即 SetByCaller 键：Spec 上 SetByCaller.ColorPigment.<颜色> 的值为该颜色的注入量，
 * 按红、蓝、黑、白的顺序输出到 UColorPigmentSet 对应的注入 meta 属性，未设置的颜色跳过；
 * 同一 Spec 注入多种颜色时后者压在前者之上。
 *
 * 只负责把 Spec 翻译成 meta 属性，外来/自身颜色的栈规则由 UColorPigmentSet 结算。
 */
UCLASS(MinimalAPI)
class UColorPigmentInjectExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

protected:
	//~Begin UGameplayEffectExecutionCalculation Interface
	UE_API virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
	//~End UGameplayEffectExecutionCalculation Interface
};

#undef UE_API
