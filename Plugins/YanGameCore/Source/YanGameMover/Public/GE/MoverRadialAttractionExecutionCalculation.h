#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"

#include "MoverRadialAttractionExecutionCalculation.generated.h"

#define UE_API YANGAMEMOVER_API

// SetByCaller 键：以浮点数编码引力中心与牵引参数，纯代码闭环故用 FName 而非 GameplayTag
extern const FName NAME_Mover_Attraction_CenterX;
extern const FName NAME_Mover_Attraction_CenterY;
extern const FName NAME_Mover_Attraction_CenterZ;
extern const FName NAME_Mover_Attraction_Radius;
extern const FName NAME_Mover_Attraction_Magnitude;
extern const FName NAME_Mover_Attraction_FalloffExponent;
extern const FName NAME_Mover_Attraction_DurationMs;

/**
 * 径向牵引 GameplayEffect 执行计算。
 *
 * 从 GE Spec 的 SetByCaller 读出引力中心与强度，转成 Authority sim action 下发到目标 Mover。
 *
 * 走 GE 而非直接调用桥接函数，是为了让「谁会被牵引」交由 GAS 裁决：
 * 免疫标签、阵营过滤、护盾等都能在 GE 资产上配置，牵引方无须自己实现一套筛选规则。
 */
UCLASS(MinimalAPI)
class UMoverRadialAttractionExecutionCalculation : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UE_API UMoverRadialAttractionExecutionCalculation();

	UE_API virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};

#undef UE_API
