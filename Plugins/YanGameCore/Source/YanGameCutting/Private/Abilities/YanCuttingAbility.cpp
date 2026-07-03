#include "Abilities/YanCuttingAbility.h"

#include "Actor/YanCuttableActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Math/RotationMatrix.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanCuttingAbility)

UYanCuttingAbility::UYanCuttingAbility()
{
	// Instanced per actor so DragStartScreenPos persists across Activate → InputReleased
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UYanCuttingAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (APlayerController* PC = ActorInfo->PlayerController.Get())
	{
		float MouseX = 0.f, MouseY = 0.f;
		PC->GetMousePosition(MouseX, MouseY);
		DragStartScreenPos = FVector2D(MouseX, MouseY);
		
		ScanForTargets(ActorInfo);
	}
}

void UYanCuttingAbility::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);
	ExecuteCut(ActorInfo);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UYanCuttingAbility::ScanForTargets(const FGameplayAbilityActorInfo* ActorInfo)
{
	RecordedTargets.Empty();

	if (!ActorInfo)
	{
		return;
	}

	APlayerController* PC = ActorInfo->PlayerController.Get();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	const FVector         CameraLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector         CameraForward = PC->PlayerCameraManager->GetCameraRotation().Vector();

	TArray<FHitResult>    OutHits;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(ActorInfo->AvatarActor.Get());

	// Perform a multi-line trace to find all cuttable actors along the line of sight at the moment of press.
	const bool bHit = GetWorld()->LineTraceMultiByChannel(
		OutHits,
		CameraLoc,
		CameraLoc + CameraForward * MaxCutDistance,
		ECC_WorldDynamic,
		QueryParams);

	if (bHit)
	{
		for (const FHitResult& Hit : OutHits)
		{
			if (AYanCuttableActor* Target = Cast<AYanCuttableActor>(Hit.GetActor()))
			{
				FYanTargetActorHandle Handle;
				Handle.TargetActor = Target;
				Handle.HitInfo = Hit;
				RecordedTargets.Add(Handle);
			}
		}
	}
}

void UYanCuttingAbility::ExecuteCut(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!ActorInfo || RecordedTargets.Num() == 0)
	{
		return;
	}

	APlayerController* PC = ActorInfo->PlayerController.Get();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	float MouseX = 0.f, MouseY = 0.f;
	if (!PC->GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	const FVector2D DragScreen = FVector2D(MouseX, MouseY) - DragStartScreenPos;

	const FRotationMatrix CamMat(PC->PlayerCameraManager->GetCameraRotation());
	const FVector         CameraForward = CamMat.GetScaledAxis(EAxis::X);
	const FVector         CameraRight   = CamMat.GetScaledAxis(EAxis::Y);
	const FVector         CameraUp      = CamMat.GetScaledAxis(EAxis::Z);

	const FVector CutNormal = ComputeCutNormal(DragScreen, CameraRight, CameraUp, CameraForward);

	for (const FYanTargetActorHandle& TargetHandle : RecordedTargets)
	{
		if (AYanCuttableActor* Target = Cast<AYanCuttableActor>(TargetHandle.TargetActor))
		{
			// Use the recorded impact point and item index for each target.
			Target->CutByPlane(TargetHandle.HitInfo.ImpactPoint, CutNormal, FMath::Max(0, TargetHandle.HitInfo.Item));
		}
	}
}

FVector UYanCuttingAbility::ComputeCutNormal(FVector2D DragScreen, FVector CameraRight, FVector CameraUp, FVector CameraForward) const
{
	if (DragScreen.Size() < MinDragPixels)
	{
		// Near-stationary click: default to a horizontal cut (right-facing normal)
		return CameraRight;
	}

	// Screen Y is down; flip to match world-space up
	const FVector2D DragNorm  = DragScreen.GetSafeNormal();
	const FVector   DragWorld =
	(DragNorm.X * CameraRight + (-DragNorm.Y) * CameraUp).GetSafeNormal();

	// Cut normal = drag direction × camera forward
	// Horizontal drag → normal points up   → horizontal slice
	// Vertical drag   → normal points right → vertical slice
	return FVector::CrossProduct(DragWorld, CameraForward).GetSafeNormal();
}
