#include "GE/MoverLaunchExecutionCalculation.h"

#include "AbilitySystemComponent.h"
#include "MoverComponent.h"
#include "GameFramework/Actor.h"
#include "DefaultMovementSet/LayeredMoves/LaunchMove.h"
#include "ChaosMover/Character/ChaosCharacterMoverComponent.h"
#include "ChaosMover/Character/Effects/ChaosCharacterApplyVelocityEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverLaunchExecutionCalculation)

// SetByCaller 键定义：每个分量一个键，规避 FVector 无法直接序列化到 GE Spec 的限制
const FName NAME_Mover_Launch_VelocityX(TEXT("Mover.Launch.VelocityX"));
const FName NAME_Mover_Launch_VelocityY(TEXT("Mover.Launch.VelocityY"));
const FName NAME_Mover_Launch_VelocityZ(TEXT("Mover.Launch.VelocityZ"));
const FName NAME_Mover_Launch_DurationMs(TEXT("Mover.Launch.DurationMs"));

UMoverLaunchExecutionCalculation::UMoverLaunchExecutionCalculation()
{}

void UMoverLaunchExecutionCalculation::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	// 从 SetByCaller 读取击飞参数
	const float VelocityX  = Spec.GetSetByCallerMagnitude(NAME_Mover_Launch_VelocityX, /*bWarnIfNotFound=*/false, 0.f);
	const float VelocityY  = Spec.GetSetByCallerMagnitude(NAME_Mover_Launch_VelocityY, false, 0.f);
	const float VelocityZ  = Spec.GetSetByCallerMagnitude(NAME_Mover_Launch_VelocityZ, false, 0.f);
	const float DurationMs = Spec.GetSetByCallerMagnitude(NAME_Mover_Launch_DurationMs, false, 0.f);

	const FVector LaunchVelocity(VelocityX, VelocityY, VelocityZ);

	// 从目标 ASC 获取目标 Actor，再找到 MoverComponent
	if (UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent())
	{
		if (AActor* TargetActor = TargetASC->GetAvatarActor())
		{
			if (UMoverComponent* TargetMover = TargetActor->FindComponentByClass<UMoverComponent>())
			{
				if (UChaosCharacterMoverComponent* ChaosMover = Cast<UChaosCharacterMoverComponent>(TargetMover))
				{
					// ChaosMover 后端不消费 LayeredMove。单次发力改用 OverrideVelocity 瞬时速度效果，
					// 经 InjectInstantMovementEffectsIntoSim 注入；向上分量足够时由默认 FallingCheck 切 Falling。
					// 瞬时速度无持续语义，DurationMs 在此忽略。
					TSharedPtr<FChaosCharacterApplyVelocityEffect> LaunchEffect = MakeShared<FChaosCharacterApplyVelocityEffect>();
					LaunchEffect->VelocityOrImpulseToApply                      = LaunchVelocity;
					LaunchEffect->Mode                                          = EChaosMoverVelocityEffectMode::OverrideVelocity;
					ChaosMover->QueueInstantMovementEffect(LaunchEffect);
				}
				else
				{
					// 标准 Mover：FLayeredMove_Launch 在 DurationMs 内施加击飞速度
					TSharedPtr<FLayeredMove_Launch> LaunchMove = MakeShared<FLayeredMove_Launch>();
					LaunchMove->LaunchVelocity                 = LaunchVelocity;
					LaunchMove->ForceMovementMode              = FName("Falling"); // 默认Falling
					LaunchMove->DurationMs                     = DurationMs;
					TargetMover->QueueLayeredMove(LaunchMove);
				}
			}
		}
	}
}
