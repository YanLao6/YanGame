#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"

#include "MoverLaunchExecutionCalculation.generated.h"

#define UE_API YANGAMEMOVER_API

class UMoverLaunchGameplayEffect;

// SetByCaller 键：以 3 个浮点数编码击飞速度向量，纯代码闭环故用 FName 而非 GameplayTag
extern const FName NAME_Mover_Launch_VelocityX;
extern const FName NAME_Mover_Launch_VelocityY;
extern const FName NAME_Mover_Launch_VelocityZ;

/**
 * 击飞 GameplayEffect 执行计算：把速度落到目标的 MoverComponent 上。
 *
 * 速度取自 GE Spec 的 SetByCaller，方向由施加方决定；大小再按目标当前负荷放大，
 * 放大曲线取自 UMoverLaunchGameplayEffect 上的配置。目标不具备 UYanImpulseSet 时
 * 退化为不缩放，故非对抗性目标无须挂该属性集。
 *
 * 通常无须直接引用本类，用 UMoverLaunchGameplayEffect 即已接好。
 */
UCLASS(MinimalAPI)
class UMoverLaunchExecutionCalculation : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UE_API UMoverLaunchExecutionCalculation();

	UE_API virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

private:
	// 由目标当前负荷求击飞速度倍率；捕获失败或目标无负荷上限时返回 1
	UE_API float CalculateLoadSpeedMultiplier(const FGameplayEffectCustomExecutionParameters& ExecutionParams, float MultiplierAtMaxLoad) const;

	// 目标已处于格挡姿态时，判定来袭方向是否落在其正面锥内
	UE_API bool IsLaunchBlocked(const AActor* TargetActor, const FVector& LaunchVelocity, const UMoverLaunchGameplayEffect* LaunchEffectDef) const;

	// 给目标挂上卸力窗口效果；未配置窗口效果时不做任何事，即本次击退无法被卸力
	UE_API void ApplyBreakWindow(UAbilitySystemComponent& TargetASC, const UMoverLaunchGameplayEffect* LaunchEffectDef) const;
};

#undef UE_API
