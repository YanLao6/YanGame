#include "GameplayCue/YanMoverLaunchHitCue.h"

#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanMoverLaunchHitCue)

bool UYanMoverLaunchHitCue::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (!MyTarget)
	{
		return false;
	}

	// 命中位置优先取 cue 参数，缺省时退回目标位置
	const FVector Location = Parameters.Location.IsNearlyZero() ? MyTarget->GetActorLocation() : FVector(Parameters.Location);

	if (HitSound)
	{
		UGameplayStatics::PlaySoundAtLocation(MyTarget, HitSound, Location);
	}

	if (HitEffect)
	{
		FTransform Transform(MyTarget->GetActorRotation(), Location);
		UGameplayStatics::SpawnEmitterAtLocation(MyTarget->GetWorld(), HitEffect, Transform);
	}

	return true;
}
