// Copyright Chronicler.

#include "CameraModes/CameraMode_Fixed.h"

#include "CameraAssistInterface.h"
#include "ModalCameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Canvas.h"
#include "Engine/DebugCameraController.h"
#include "GameFramework/CameraBlockingVolume.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CameraMode_Fixed)

namespace CameraMode_Fixed_Statics
{
	static const FName NAME_IgnoreCameraCollision = TEXT("IgnoreCameraCollision");
}

UCameraMode_Fixed::UCameraMode_Fixed()
	: AimLineToDesiredPosBlockedPct(0) { }

void UCameraMode_Fixed::OnActivation()
{
	TArray<AActor*> DebugCameras;
	UGameplayStatics::GetAllActorsOfClass(UModalCameraMode::GetWorld(), ADebugCameraController::StaticClass(), DebugCameras);
	if (DebugCameras.Num() > 0)
	{
		if (ADebugCameraController* DebugCameraController = Cast<ADebugCameraController>(DebugCameras.Last()))
		{
			DebugCameraController->GetPlayerViewPoint(FixedLocation, FixedRotation);
			// Fixed 模式取代 Debug 相机：立刻 Destroy 以免后续状态混乱
			DebugCameraController->Destroy();
			return;
		}
	}
	GetCameraComponent()->GetController<APlayerController>()->GetPlayerViewPoint(FixedLocation, FixedRotation);
}

void UCameraMode_Fixed::UpdateView(const float DeltaTime)
{
	const FVector PivotLocation = GetPivotLocation();
	FRotator PivotRotation = GetPivotRotation();

	PivotRotation.Pitch = FMath::ClampAngle(PivotRotation.Pitch, ViewPitchMin, ViewPitchMax);

	View.Location = PivotLocation;
	View.Rotation = PivotRotation;
	View.ControlRotation = View.Rotation;
	View.FieldOfView = FieldOfView;

	// 最终相机位置 Penetration 修正
	UpdatePreventPenetration(DeltaTime);
}

void UCameraMode_Fixed::DrawDebug(UCanvas* Canvas) const
{
	Super::DrawDebug(Canvas);

#if ENABLE_DRAW_DEBUG
	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
	for (int i = 0; i < DebugActorsHitDuringCameraPenetration.Num(); i++)
	{
		DisplayDebugManager.DrawString(
			FString::Printf(TEXT("HitActorDuringPenetration[%d]: %s")
				, i
				, *DebugActorsHitDuringCameraPenetration[i]->GetName()));
	}

	LastDrawDebugTime = GetWorld()->GetTimeSeconds();
#endif
}

FVector UCameraMode_Fixed::GetPivotLocation() const
{
	return FixedLocation;
}

FRotator UCameraMode_Fixed::GetPivotRotation() const
{
	return FixedRotation;
}

void UCameraMode_Fixed::UpdatePreventPenetration(float DeltaTime)
{
	if (!bPreventPenetration)
	{
		return;
	}

	AActor* TargetActor = GetTargetActor();

	const APawn* TargetPawn = Cast<APawn>(TargetActor);
	AController* TargetController = TargetPawn ? TargetPawn->GetController() : nullptr;
	ICameraAssistInterface* TargetControllerAssist = Cast<ICameraAssistInterface>(TargetController);

	ICameraAssistInterface* TargetActorAssist = Cast<ICameraAssistInterface>(TargetActor);

	TOptional<AActor*> OptionalPPTarget = TargetActorAssist ? TargetActorAssist->GetCameraPreventPenetrationTarget() : TOptional<AActor*>();
	AActor* PPActor = OptionalPPTarget.IsSet() ? OptionalPPTarget.GetValue() : TargetActor;
	ICameraAssistInterface* PPActorAssist = OptionalPPTarget.IsSet() ? Cast<ICameraAssistInterface>(PPActor) : nullptr;

	if (const UPrimitiveComponent* PPActorRootComponent = Cast<UPrimitiveComponent>(PPActor->GetRootComponent()))
	{
		// 自动选取 SafeLocation：瞄准线尽量稳，减少相机平移；取 Capsule 上距瞄准线最近点
		FVector ClosestPointOnLineToCapsuleCenter;
		FVector SafeLocation = PPActor->GetActorLocation();
		FMath::PointDistToLine(SafeLocation, View.Rotation.Vector(), View.Location, ClosestPointOnLineToCapsuleCenter);

		// Z 与瞄准线对齐但限制在 Capsule 半高内
		float const PushInDistance = CollisionPushOutDistance;
		float const MaxHalfHeight = PPActor->GetSimpleCollisionHalfHeight() - PushInDistance;
		SafeLocation.Z = FMath::Clamp(ClosestPointOnLineToCapsuleCenter.Z, SafeLocation.Z - MaxHalfHeight, SafeLocation.Z + MaxHalfHeight);

		float DistanceSqr;
		PPActorRootComponent->GetSquaredDistanceToCollision(ClosestPointOnLineToCapsuleCenter, DistanceSqr, SafeLocation);

		// SafeLocation → 期望相机位置 的 Penetration 处理
		PreventCameraPenetration(*PPActor, SafeLocation, View.Location, DeltaTime, AimLineToDesiredPosBlockedPct, true);

		ICameraAssistInterface* AssistArray[] = { TargetControllerAssist, TargetActorAssist, PPActorAssist };

		if (AimLineToDesiredPosBlockedPct < ReportPenetrationPercent)
		{
			for (ICameraAssistInterface* Assist : AssistArray)
			{
				if (Assist)
				{
					// 距离过近，通知 Assist 接口
					Assist->OnCameraPenetratingTarget();
				}
			}
		}
	}
}

void UCameraMode_Fixed::PreventCameraPenetration(class AActor const& ViewTarget, FVector const& SafeLoc, FVector& CameraLoc, float const& DeltaTime, float& DistBlockedPct, bool bSingleRayOnly)
{
#if ENABLE_DRAW_DEBUG
	DebugActorsHitDuringCameraPenetration.Reset();
#endif

	float HardBlockedPct = DistBlockedPct;
	float SoftBlockedPct = DistBlockedPct;

	FVector BaseRay = CameraLoc - SafeLoc;
	FRotationMatrix BaseRayMatrix(BaseRay.Rotation());
	FVector BaseRayLocalUp, BaseRayLocalFwd, BaseRayLocalRight;

	BaseRayMatrix.GetScaledAxes(BaseRayLocalFwd, BaseRayLocalRight, BaseRayLocalUp);

	float DistBlockedPctThisFrame = 1.f;

	int32 const NumRaysToShoot = bSingleRayOnly ? 1 : 4;
	FCollisionQueryParams SphereParams(SCENE_QUERY_STAT(CameraPen), false, nullptr/*PlayerCamera*/);

	SphereParams.AddIgnoredActor(&ViewTarget);
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(0.f);
	UWorld* World = GetWorld();

	for (int32 RayIdx = 0; RayIdx < NumRaysToShoot; ++RayIdx)
	{
		// 射线终点（Fixed 模式为主射线指向 SafeLoc）
		FVector RayTarget;
		{
			RayTarget = SafeLoc;
		}

		ECollisionChannel TraceChannel = ECC_Camera;		//(Feeler.PawnWeight > 0.f) ? ECC_Pawn : ECC_Camera;

		// Sweep 排除虚假前景命中，避免挡住后方真实碰撞

		// CameraBlockingVolume 等需识别为相机发起的 Trace（SphereParams 可扩展）
		FHitResult Hit;
		const bool bHit = World->SweepSingleByChannel(Hit, SafeLoc, RayTarget, FQuat::Identity, TraceChannel, SphereShape, SphereParams);
#if ENABLE_DRAW_DEBUG
		if (World->TimeSince(LastDrawDebugTime) < 1.f)
		{
			DrawDebugSphere(World, SafeLoc, SphereShape.Sphere.Radius, 8, FColor::Red);
			DrawDebugSphere(World, bHit ? Hit.Location : RayTarget, SphereShape.Sphere.Radius, 8, FColor::Red);
			DrawDebugLine(World, SafeLoc, bHit ? Hit.Location : RayTarget, FColor::Red);
		}
#endif // ENABLE_DRAW_DEBUG


		if (const AActor* HitActor = Hit.GetActor(); bHit && HitActor)
		{
			bool bIgnoreHit = false;

			if (HitActor->ActorHasTag(CameraMode_Fixed_Statics::NAME_IgnoreCameraCollision))
			{
				bIgnoreHit = true;
				SphereParams.AddIgnoredActor(HitActor);
			}

			// ViewTarget 前方的 CameraBlockingVolume 命中忽略（避免误拉相机）
			if (!bIgnoreHit && HitActor->IsA<ACameraBlockingVolume>())
			{
				const FVector ViewTargetForwardXY = ViewTarget.GetActorForwardVector().GetSafeNormal2D();
				const FVector ViewTargetLocation = ViewTarget.GetActorLocation();
				const FVector HitOffset = Hit.Location - ViewTargetLocation;
				const FVector HitDirectionXY = HitOffset.GetSafeNormal2D();
				if (const float DotHitDirection = FVector::DotProduct(ViewTargetForwardXY, HitDirectionXY); DotHitDirection > 0.0f)
				{
					bIgnoreHit = true;
					// 后续 Sweep 继续忽略该 Volume
					SphereParams.AddIgnoredActor(HitActor);
				}
				else
				{
#if ENABLE_DRAW_DEBUG
					DebugActorsHitDuringCameraPenetration.AddUnique(TObjectPtr<const AActor>(HitActor));
#endif
				}
			}

			if (!bIgnoreHit)
			{
				float NewBlockPct = Hit.Time;
				NewBlockPct += (1.f - NewBlockPct);

				// 结合 CollisionPushOutDistance 重算阻塞比例
				NewBlockPct = ((Hit.Location - SafeLoc).Size() - CollisionPushOutDistance) / (RayTarget - SafeLoc).Size();
				DistBlockedPctThisFrame = FMath::Min(NewBlockPct, DistBlockedPctThisFrame);

#if ENABLE_DRAW_DEBUG
				DebugActorsHitDuringCameraPenetration.AddUnique(TObjectPtr<const AActor>(HitActor));
#endif
			}
		}

		if (RayIdx == 0)
		{
			// 主射线：不向其插值，直接采用（Hard）
			HardBlockedPct = DistBlockedPctThisFrame;
		}
		else
		{
			SoftBlockedPct = DistBlockedPctThisFrame;
		}
	}

	if (bResetInterpolation)
	{
		DistBlockedPct = DistBlockedPctThisFrame;
	}
	else if (DistBlockedPct < DistBlockedPctThisFrame)
	{
			DistBlockedPct = DistBlockedPctThisFrame;
	}
	else
	{
		if (DistBlockedPct > HardBlockedPct)
		{
			DistBlockedPct = HardBlockedPct;
		}
		else if (DistBlockedPct > SoftBlockedPct)
		{
			DistBlockedPct = SoftBlockedPct;
		}
	}

	DistBlockedPct = FMath::Clamp<float>(DistBlockedPct, 0.f, 1.f);
	if (DistBlockedPct < (1.f - ZERO_ANIMWEIGHT_THRESH))
	{
		CameraLoc = SafeLoc + (CameraLoc - SafeLoc) * DistBlockedPct;
	}
}
