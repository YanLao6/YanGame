#include "GE/MoverLaunchExecutionCalculation.h"

#include "AbilitySystemComponent.h"
#include "MoverComponent.h"
#include "YanMoverAngelscriptLibrary.h"
#include "Abilities/YanBlockAbility.h"
#include "AttributeSets/YanImpulseSet.h"
#include "GameFramework/Pawn.h"
#include "GE/MoverLaunchGameplayEffect.h"
#include "MoverMode/Sliding/ChaosSlidingMode.h"
#include "MoverMode/WallClimb/ChaosWallClimbMode.h"
#include "GameFramework/Actor.h"
#include "DefaultMovementSet/LayeredMoves/LaunchMove.h"
#include "ChaosMover/Character/ChaosCharacterMoverComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverLaunchExecutionCalculation)

// SetByCaller 键定义：每个分量一个键，规避 FVector 无法直接序列化到 GE Spec 的限制
const FName NAME_Mover_Launch_VelocityX(TEXT("Mover.Launch.VelocityX"));
const FName NAME_Mover_Launch_VelocityY(TEXT("Mover.Launch.VelocityY"));
const FName NAME_Mover_Launch_VelocityZ(TEXT("Mover.Launch.VelocityZ"));

namespace
{
	/**
	 * 负荷属性捕获定义。
	 *
	 * 均取目标当前值而非快照：负荷在命中当帧才由伤害流程累加，快照会读到累加前的旧值。
	 */
	struct FLaunchLoadCapture
	{
		FGameplayEffectAttributeCaptureDefinition LoadDef;
		FGameplayEffectAttributeCaptureDefinition MaxLoadDef;

		FLaunchLoadCapture()
		{
			LoadDef    = FGameplayEffectAttributeCaptureDefinition(UYanImpulseSet::GetLoadAttribute(), EGameplayEffectAttributeCaptureSource::Target, /*bSnapshot=*/false);
			MaxLoadDef = FGameplayEffectAttributeCaptureDefinition(UYanImpulseSet::GetMaxLoadAttribute(), EGameplayEffectAttributeCaptureSource::Target, /*bSnapshot=*/false);
		}
	};

	const FLaunchLoadCapture& GetLaunchLoadCapture()
	{
		static FLaunchLoadCapture Capture;
		return Capture;
	}
}

UMoverLaunchExecutionCalculation::UMoverLaunchExecutionCalculation()
{
	RelevantAttributesToCapture.Add(GetLaunchLoadCapture().LoadDef);
	RelevantAttributesToCapture.Add(GetLaunchLoadCapture().MaxLoadDef);
}

float UMoverLaunchExecutionCalculation::CalculateLoadSpeedMultiplier(const FGameplayEffectCustomExecutionParameters& ExecutionParams, float MultiplierAtMaxLoad) const
{
	if (FMath::IsNearlyEqual(MultiplierAtMaxLoad, 1.0f))
	{
		return 1.0f;
	}

	FAggregatorEvaluateParameters EvaluateParams;
	EvaluateParams.SourceTags = ExecutionParams.GetOwningSpec().CapturedSourceTags.GetAggregatedTags();
	EvaluateParams.TargetTags = ExecutionParams.GetOwningSpec().CapturedTargetTags.GetAggregatedTags();

	// 目标未挂 UYanImpulseSet 时捕获失败，此处退化为不缩放而非视作满负荷
	float Load = 0.0f;
	if (!ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(GetLaunchLoadCapture().LoadDef, EvaluateParams, Load))
	{
		return 1.0f;
	}

	float MaxLoad = 0.0f;
	if (!ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(GetLaunchLoadCapture().MaxLoadDef, EvaluateParams, MaxLoad) || MaxLoad <= 0.0f)
	{
		return 1.0f;
	}

	const float LoadRatio = FMath::Clamp(Load / MaxLoad, 0.0f, 1.0f);
	return FMath::Lerp(1.0f, MultiplierAtMaxLoad, LoadRatio);
}

bool UMoverLaunchExecutionCalculation::IsLaunchBlocked(const AActor* TargetActor, const FVector& LaunchVelocity, const UMoverLaunchGameplayEffect* LaunchEffectDef) const
{
	if (TargetActor == nullptr)
	{
		return false;
	}

	// 来袭方向由击飞方向反推，而非取施加者位置：投射物与回拉的发出点都不等于推力方向
	const FVector IncomingDirection = (-LaunchVelocity).GetSafeNormal2D();
	if (IncomingDirection.IsNearlyZero())
	{
		// 纯垂直击飞在水平面上没有来袭方向可言，一律视作无法格挡
		return false;
	}

	// 取瞄准朝向而非 Actor 朝向：Actor 可能被配置为朝向移动方向，那不是玩家认为自己面朝的方向
	FVector FacingDirection = TargetActor->GetActorForwardVector();
	if (const APawn* TargetPawn = Cast<APawn>(TargetActor))
	{
		FacingDirection = TargetPawn->GetBaseAimRotation().Vector();
	}

	FacingDirection = FacingDirection.GetSafeNormal2D();
	if (FacingDirection.IsNearlyZero())
	{
		return false;
	}

	const float ConeDegrees = LaunchEffectDef ? LaunchEffectDef->BlockFacingConeDegrees : 0.f;
	if (ConeDegrees <= 0.f)
	{
		return false;
	}

	// ConeDegrees 是张角全宽，故与半角的余弦比较
	return FVector::DotProduct(FacingDirection, IncomingDirection) >= FMath::Cos(FMath::DegreesToRadians(ConeDegrees * 0.5f));
}

void UMoverLaunchExecutionCalculation::ApplyBreakWindow(UAbilitySystemComponent& TargetASC, const UMoverLaunchGameplayEffect* LaunchEffectDef) const
{
	if (LaunchEffectDef == nullptr || !LaunchEffectDef->BreakWindowEffect)
	{
		return;
	}

	// 窗口由目标自己承载而非来自施加者：它描述的是「受击方现在可以卸力」这一状态，
	// 与谁打的无关，故用 ApplyGameplayEffectSpecToSelf 而不经施加者 ASC 转手
	const FGameplayEffectSpecHandle WindowSpec = TargetASC.MakeOutgoingSpec(LaunchEffectDef->BreakWindowEffect, 1.f, TargetASC.MakeEffectContext());
	if (WindowSpec.IsValid())
	{
		TargetASC.ApplyGameplayEffectSpecToSelf(*WindowSpec.Data.Get());
	}
}

void UMoverLaunchExecutionCalculation::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	// 速度逐目标变化，只能由施加方在命中当帧经 SetByCaller 传入
	const float VelocityX = Spec.GetSetByCallerMagnitude(NAME_Mover_Launch_VelocityX, /*bWarnIfNotFound=*/false, 0.f);
	const float VelocityY = Spec.GetSetByCallerMagnitude(NAME_Mover_Launch_VelocityY, false, 0.f);
	const float VelocityZ = Spec.GetSetByCallerMagnitude(NAME_Mover_Launch_VelocityZ, false, 0.f);

	// 施加方式随效果定死，故配在 GE 上。非 UMoverLaunchGameplayEffect 的既有资产取与其默认值一致的行为
	const UMoverLaunchGameplayEffect* LaunchEffectDef  = Cast<UMoverLaunchGameplayEffect>(Spec.Def.Get());
	const float                       DurationMs       = LaunchEffectDef ? LaunchEffectDef->LaunchDurationMs : 0.f;
	const FName                       MovementMode     = LaunchEffectDef ? LaunchEffectDef->ForceMovementMode : FName("Falling");
	const bool                        bOverrideVelocity = LaunchEffectDef ? LaunchEffectDef->bOverrideVelocity : true;
	const float                       MultiplierAtMax   = LaunchEffectDef ? LaunchEffectDef->SpeedMultiplierAtMaxLoad : 1.f;

	// 只缩放大小不动方向：方向承载施加方的命中语义，负荷只决定推得多远
	const FVector LaunchVelocity = FVector(VelocityX, VelocityY, VelocityZ) * CalculateLoadSpeedMultiplier(ExecutionParams, MultiplierAtMax);

	// 从目标 ASC 获取目标 Actor，再找到 MoverComponent
	if (UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent())
	{
		if (AActor* TargetActor = TargetASC->GetAvatarActor())
		{
			// 格挡成立即完全吃掉本次击飞：不再触碰 Mover，改为回补机动能量
			if (TargetASC->HasMatchingGameplayTag(Status_Blocking) && IsLaunchBlocked(TargetActor, LaunchVelocity, LaunchEffectDef))
			{
				const float Refund = LaunchEffectDef ? LaunchEffectDef->ChargeRefundOnBlock : 0.f;
				if (Refund > 0.f)
				{
					// 走 output modifier 而非直接 SetCharge，才能经过属性集的整格 clamp 与变化广播
					OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UYanImpulseSet::GetChargeAttribute(), EGameplayModOp::Additive, Refund));
				}
				return;
			}

			// 击退成立即抽走目标一格能量并开出卸力窗口。两者都不取决于击退是否真的推动了目标——
			// 目标即便被墙卡住没飞出去，这一击同样应当削弱他的逃生能力
			if (LaunchEffectDef != nullptr && LaunchEffectDef->ChargePenaltyOnHit > 0.f)
			{
				OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UYanImpulseSet::GetChargeAttribute(), EGameplayModOp::Additive, -LaunchEffectDef->ChargePenaltyOnHit));
			}

			ApplyBreakWindow(*TargetASC, LaunchEffectDef);

			// 打断先于施力：钩锁飞行中的目标须先脱钩，否则这一帧的击退还会被钩索继续拖回去
			if (LaunchEffectDef != nullptr && !LaunchEffectDef->AbilityTagsToCancelOnHit.IsEmpty())
			{
				TargetASC->CancelAbilities(&LaunchEffectDef->AbilityTagsToCancelOnHit);
			}

			if (UMoverComponent* TargetMover = TargetActor->FindComponentByClass<UMoverComponent>())
			{
				// 低摩擦滑行没有摩擦去吃掉这股速度，同一次击退因而推得更远。
				// 这是滑行那份「加速快、难预判」的收益所对应的代价，也是最容易一击出界的时刻
				const float SlidingMultiplier = (LaunchEffectDef != nullptr && TargetMover->HasGameplayTag(Mover_IsSliding, /*bExactMatch=*/true))
					                                ? LaunchEffectDef->SpeedMultiplierWhileSliding
					                                : 1.f;
				FVector FinalVelocity = LaunchVelocity * SlidingMultiplier;

				// 附墙者被击中却仍挂在原处，会让墙面变成免疫区。强制脱墙并压一段向下速度，
				// 使「被打下来」真的落回下坠状态而非原地弹开
				if (LaunchEffectDef != nullptr && TargetMover->HasGameplayTag(Mover_IsWallClimbing, /*bExactMatch=*/true))
				{
					FinalVelocity -= TargetMover->GetUpDirection() * LaunchEffectDef->WallDetachDownwardSpeed;

					if (LaunchEffectDef->WallDetachNoLandingSeconds > 0.f)
					{
						UYanMoverAngelscriptLibrary::ApplyNoLandingToTarget(TargetMover, LaunchEffectDef->WallDetachNoLandingSeconds);
					}
				}

				if (Cast<UChaosCharacterMoverComponent>(TargetMover))
				{
					// 效果作用于他人：必须以 Authority sim action 下发，目标端才不会因未预测此次速度
					// 变更而被物理修正。向上分量足够时由默认 FallingCheck 切 Falling。
					// 瞬时速度无持续语义，DurationMs 与 ForceMovementMode 在此均无从施加。
					UYanMoverAngelscriptLibrary::ApplyAuthoritativeVelocityToTarget(TargetMover, FinalVelocity, bOverrideVelocity, /*bScheduleForSync=*/true);
				}
				else
				{
					// 标准 Mover：FLayeredMove_Launch 在 DurationMs 内施加击飞速度
					TSharedPtr<FLayeredMove_Launch> LaunchMove = MakeShared<FLayeredMove_Launch>();
					LaunchMove->LaunchVelocity                 = FinalVelocity;
					LaunchMove->ForceMovementMode              = MovementMode;
					LaunchMove->DurationMs                     = DurationMs;
					TargetMover->QueueLayeredMove(LaunchMove);
				}
			}
		}
	}
}
