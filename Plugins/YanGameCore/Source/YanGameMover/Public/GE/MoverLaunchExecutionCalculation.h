#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"

#include "MoverLaunchExecutionCalculation.generated.h"

// SetByCaller 键：以 4 个浮点数编码击飞速度向量和持续时间，纯代码闭环故用 FName 而非 GameplayTag
extern const FName NAME_Mover_Launch_VelocityX;
extern const FName NAME_Mover_Launch_VelocityY;
extern const FName NAME_Mover_Launch_VelocityZ;
extern const FName NAME_Mover_Launch_DurationMs;

/**
 * 击飞 GameplayEffect 执行计算。
 * 从 GE Spec 的 SetByCaller 中读取速度分量和持续时间，
 */
UCLASS()
class YANGAMEMOVER_API UMoverLaunchExecutionCalculation : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UMoverLaunchExecutionCalculation();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
