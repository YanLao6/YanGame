#include "GE/MoverRadialAttractionExecutionCalculation.h"

#include "AbilitySystemComponent.h"
#include "MoverComponent.h"
#include "YanMoverAngelscriptLibrary.h"
#include "GameFramework/Actor.h"
#include "ChaosMover/Character/ChaosCharacterMoverComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverRadialAttractionExecutionCalculation)

const FName NAME_Mover_Attraction_CenterX(TEXT("Mover.Attraction.CenterX"));
const FName NAME_Mover_Attraction_CenterY(TEXT("Mover.Attraction.CenterY"));
const FName NAME_Mover_Attraction_CenterZ(TEXT("Mover.Attraction.CenterZ"));
const FName NAME_Mover_Attraction_Radius(TEXT("Mover.Attraction.Radius"));
const FName NAME_Mover_Attraction_Magnitude(TEXT("Mover.Attraction.Magnitude"));
const FName NAME_Mover_Attraction_FalloffExponent(TEXT("Mover.Attraction.FalloffExponent"));
const FName NAME_Mover_Attraction_DurationMs(TEXT("Mover.Attraction.DurationMs"));

UMoverRadialAttractionExecutionCalculation::UMoverRadialAttractionExecutionCalculation()
{
}

void UMoverRadialAttractionExecutionCalculation::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	const FVector Center(Spec.GetSetByCallerMagnitude(NAME_Mover_Attraction_CenterX, /*bWarnIfNotFound=*/false, 0.f),
	                     Spec.GetSetByCallerMagnitude(NAME_Mover_Attraction_CenterY, false, 0.f),
	                     Spec.GetSetByCallerMagnitude(NAME_Mover_Attraction_CenterZ, false, 0.f));

	const float Radius          = Spec.GetSetByCallerMagnitude(NAME_Mover_Attraction_Radius, false, 0.f);
	const float Magnitude       = Spec.GetSetByCallerMagnitude(NAME_Mover_Attraction_Magnitude, false, 0.f);
	const float FalloffExponent = Spec.GetSetByCallerMagnitude(NAME_Mover_Attraction_FalloffExponent, false, 1.f);
	const float DurationMs      = Spec.GetSetByCallerMagnitude(NAME_Mover_Attraction_DurationMs, false, 0.f);

	if (Radius <= 0.f || Magnitude <= 0.f || DurationMs <= 0.f)
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	AActor*                  TargetActor = TargetASC ? TargetASC->GetAvatarActor() : nullptr;
	UMoverComponent*         TargetMover = TargetActor ? TargetActor->FindComponentByClass<UMoverComponent>() : nullptr;

	if (!TargetMover)
	{
		return;
	}

	// 牵引依赖异步仿真下的 LayeredMove 通道，非 Chaos 后端的 Mover 走不到这条路
	if (!Cast<UChaosCharacterMoverComponent>(TargetMover))
	{
		return;
	}

	UYanMoverAngelscriptLibrary::ApplyRadialAttractionToTarget(TargetMover, Center, Radius, Magnitude, FalloffExponent, DurationMs, /*bIsPush=*/false);
}
