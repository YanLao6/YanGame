#include "Executions/YanDamageExecution.h"

#include "AttributeSets/YanBaseCombatSet.h"
#include "AttributeSets/YanHealthSet.h"
#include "GameplayAbilities/ModularAbilitySourceInterface.h"
#include "GameplayAbilities/ModularGameplayEffectContext.h"
#include "ModularGameplayAbilitiesLogChannels.h"
#include "ModularTeamSubsystem.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanDamageExecution)

namespace
{
	struct FYanDamageStatics
	{
		FGameplayEffectAttributeCaptureDefinition BaseDamageDef;

		FYanDamageStatics()
		{
			BaseDamageDef = FGameplayEffectAttributeCaptureDefinition(UYanBaseCombatSet::GetBaseDamageAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);
		}
	};

	const FYanDamageStatics& DamageStatics()
	{
		static FYanDamageStatics Statics;
		return Statics;
	}
}

UYanDamageExecution::UYanDamageExecution()
{
	RelevantAttributesToCapture.Add(DamageStatics().BaseDamageDef);
}

void UYanDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
#if WITH_SERVER_CODE
	const FGameplayEffectSpec&       Spec         = ExecutionParams.GetOwningSpec();
	FModularGameplayEffectContext*   TypedContext = FModularGameplayEffectContext::ExtractEffectContext(Spec.GetContext());
	check(TypedContext);

	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	FAggregatorEvaluateParameters EvaluateParameters;
	EvaluateParameters.SourceTags = SourceTags;
	EvaluateParameters.TargetTags = TargetTags;

	float BaseDamage = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().BaseDamageDef, EvaluateParameters, BaseDamage);

	const AActor*     EffectCauser   = TypedContext->GetEffectCauser();
	const FHitResult* HitActorResult = TypedContext->GetHitResult();

	AActor* HitActor       = nullptr;
	FVector ImpactLocation = FVector::ZeroVector;

	// 命中 Actor、表面与距离均依赖 HitResult；直接施加的效果没有 HitResult，需回退到目标 Avatar
	if (HitActorResult)
	{
		HitActor = HitActorResult->HitObjectHandle.FetchActor();
		if (HitActor)
		{
			ImpactLocation = HitActorResult->ImpactPoint;
		}
	}

	if (!HitActor)
	{
		UAbilitySystemComponent* TargetAbilitySystemComponent = ExecutionParams.GetTargetAbilitySystemComponent();
		HitActor                                              = TargetAbilitySystemComponent ? TargetAbilitySystemComponent->GetAvatarActor_Direct() : nullptr;
		if (HitActor)
		{
			ImpactLocation = HitActor->GetActorLocation();
		}
	}

	// 队伍规则决定伤害是否成立（友伤、自伤等）
	float DamageInteractionAllowedMultiplier = 0.0f;
	if (HitActor)
	{
		UModularTeamSubsystem* TeamSubsystem = HitActor->GetWorld()->GetSubsystem<UModularTeamSubsystem>();
		if (ensure(TeamSubsystem))
		{
			DamageInteractionAllowedMultiplier = TeamSubsystem->CanCauseDamage(EffectCauser, HitActor) ? 1.0f : 0.0f;
		}
	}

	double Distance = WORLD_MAX;

	if (TypedContext->HasOrigin())
	{
		Distance = FVector::Dist(TypedContext->GetOrigin(), ImpactLocation);
	}
	else if (EffectCauser)
	{
		Distance = FVector::Dist(EffectCauser->GetActorLocation(), ImpactLocation);
	}
	else
	{
		UE_LOG(LogModularGameplayAbilities, Error, TEXT("伤害结算无法推断 %s 的伤害来源位置，回退为 WORLD_MAX 距离"), *GetPathNameSafe(Spec.Def));
	}

	float PhysicalMaterialAttenuation = 1.0f;
	float DistanceAttenuation         = 1.0f;
	if (const IModularAbilitySourceInterface* AbilitySource = TypedContext->GetAbilitySource())
	{
		if (const UPhysicalMaterial* PhysMat = TypedContext->GetPhysicalMaterial())
		{
			PhysicalMaterialAttenuation = AbilitySource->GetPhysicalMaterialAttenuation(PhysMat, SourceTags, TargetTags);
		}

		DistanceAttenuation = AbilitySource->GetDistanceAttenuation(Distance, SourceTags, TargetTags);
	}
	DistanceAttenuation = FMath::Max(DistanceAttenuation, 0.0f);

	// 上限钳制交由生命属性集在 Damage 换算为 -Health 时处理
	const float DamageDone = FMath::Max(BaseDamage * DistanceAttenuation * PhysicalMaterialAttenuation * DamageInteractionAllowedMultiplier, 0.0f);

	if (DamageDone > 0.0f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UYanHealthSet::GetDamageAttribute(), EGameplayModOp::Additive, DamageDone));
	}
#endif // WITH_SERVER_CODE
}
