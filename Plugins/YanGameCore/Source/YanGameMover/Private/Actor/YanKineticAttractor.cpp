#include "Actor/YanKineticAttractor.h"

#include "GE/MoverRadialAttractionExecutionCalculation.h"
#include "AbilitySystemComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Messages/VerbMessageHelpers.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanKineticAttractor)

AYanKineticAttractor::AYanKineticAttractor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AYanKineticAttractor::OnLaunched()
{
	Super::OnLaunched();

	// 牵引的裁决与下发一律在服务器；客户端的本地模拟只负责弹道表现
	if (!HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		// 首拍立即施加：射出后要等一个间隔才开始拽，近处的目标会明显漏掉一段
		ApplyAttractionPulse();

		World->GetTimerManager().SetTimer(AttractionTimer, this, &AYanKineticAttractor::ApplyAttractionPulse, AttractionInterval, /*bLoop=*/true);
	}
}

void AYanKineticAttractor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttractionTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void AYanKineticAttractor::ApplyAttractionPulse()
{
	UWorld* World      = GetWorld();
	AActor* OwnerActor = GetOwner();

	if (!World || !AttractionEffect)
	{
		return;
	}

	UAbilitySystemComponent* InstigatorASC = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(OwnerActor);
	if (!InstigatorASC)
	{
		return;
	}

	const FVector Center = GetActorLocation();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(YanKineticAttraction), /*bTraceComplex=*/false, this);
	QueryParams.AddIgnoredActor(OwnerActor);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps,
	                                Center,
	                                FQuat::Identity,
	                                FCollisionObjectQueryParams(ECC_Pawn),
	                                FCollisionShape::MakeSphere(AttractionRadius),
	                                QueryParams);

	// 时长盖过一拍：刷新有抖动时牵引也不会出现空档，下一拍照常覆盖
	const float PulseDurationMs = AttractionInterval * 2000.f;
	const float Magnitude       = ResolveEffectMagnitude(AttractionMagnitude);

	TArray<AActor*> AffectedActors;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* TargetActor = Overlap.GetActor();

		// 同一 Actor 可能有多个碰撞体命中，去重后再施加
		if (!IsValid(TargetActor) || TargetActor == this || TargetActor == OwnerActor || AffectedActors.Contains(TargetActor))
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(TargetActor);
		if (!TargetASC)
		{
			continue;
		}

		FGameplayEffectContextHandle Context = InstigatorASC->MakeEffectContext();
		Context.AddInstigator(OwnerActor, this);

		FGameplayEffectSpecHandle Spec = InstigatorASC->MakeOutgoingSpec(AttractionEffect, 1.f, Context);
		if (!Spec.IsValid())
		{
			continue;
		}

		// 引力中心拆成三个浮点 SetByCaller，规避 GE Spec 无法直接携带向量的限制
		Spec.Data->SetSetByCallerMagnitude(NAME_Mover_Attraction_CenterX, Center.X);
		Spec.Data->SetSetByCallerMagnitude(NAME_Mover_Attraction_CenterY, Center.Y);
		Spec.Data->SetSetByCallerMagnitude(NAME_Mover_Attraction_CenterZ, Center.Z);
		Spec.Data->SetSetByCallerMagnitude(NAME_Mover_Attraction_Radius, AttractionRadius);
		Spec.Data->SetSetByCallerMagnitude(NAME_Mover_Attraction_Magnitude, Magnitude);
		Spec.Data->SetSetByCallerMagnitude(NAME_Mover_Attraction_FalloffExponent, FalloffExponent);
		Spec.Data->SetSetByCallerMagnitude(NAME_Mover_Attraction_DurationMs, PulseDurationMs);

		InstigatorASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);

		AffectedActors.Add(TargetActor);

		if (AffectedActors.Num() >= MaxAttractionTargets)
		{
			break;
		}
	}
}
