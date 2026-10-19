#include "Actors/ColorShotBeam.h"

#include "Attributes/ColorPigmentSet.h"
#include "ColorTransferQueries.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorShotBeam)

namespace ColorShotBeam
{
	const FName ColorParam(TEXT("Color"));
	const FName EmissiveParam(TEXT("EmissiveStrength"));
	const FName ImpactColorParam(TEXT("User.Color"));
	const FName ImpactSpawnCountParam(TEXT("User.SpawnCount"));

	// 约 0.5°，小于此变化不重发瞄准方向
	constexpr float AimResendDotThreshold = 0.99996f;
}

AColorShotBeam::AColorShotBeam()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	bReplicates = true;
	bNetUseOwnerRelevancy = true;
	SetReplicatingMovement(false);

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

bool AColorShotBeam::PredictPath(const UWorld* World, AActor* Shooter, const FVector& Start, const FVector& Direction, float MaxRange, TArray<FVector>& OutPoints, FHitResult& OutHit)
{
	const FVector End = Start + Direction.GetSafeNormal() * MaxRange;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(ColorShotBeam), false, Shooter);
	const bool bHit = World->LineTraceSingleByChannel(OutHit, Start, End, ColorTransfer::ShotTraceChannel, Params);

	OutPoints.Reset(2);
	OutPoints.Add(Start);
	OutPoints.Add(bHit ? OutHit.Location : End);
	return bHit;
}

void AColorShotBeam::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
		return;
	}

	BeamMaterial = UMaterialInstanceDynamic::Create(SegmentMaterial.LoadSynchronous(), this);
	LoadedImpactEffect = ImpactEffect.LoadSynchronous();
	LoadedImpactSound = ImpactSound.LoadSynchronous();
	OnRep_Pigment();
}

void AColorShotBeam::AuthSetPigment(EColorPigment InPigment)
{
	if (Pigment != InPigment)
	{
		Pigment = InPigment;
		OnRep_Pigment();
	}
}

void AColorShotBeam::ServerSetAimDirection_Implementation(FVector_NetQuantizeNormal InAimDirection)
{
	if (!InAimDirection.IsNearlyZero())
	{
		AimDirection = InAimDirection.GetSafeNormal();
	}
}

FVector AColorShotBeam::ComputeLocalAimDirection(const APawn* Shooter) const
{
	const FVector ViewLocation = Shooter->GetPawnViewLocation();

	FVector CameraStart;
	FVector CameraDirection;
	if (!ColorTransfer::GetLocalCameraAim(Shooter, /*bStartAtPawn=*/true, CameraStart, CameraDirection))
	{
		return Shooter->GetController() ? Shooter->GetController()->GetControlRotation().Vector() : Shooter->GetActorForwardVector();
	}

	// 焦点取相机中心射线在射程内的命中点，未命中则取射程末端，射线从视点指向它才与准星重合
	const FVector CameraEnd = CameraStart + CameraDirection * MaxRange;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(ColorShotBeamAim), false, Shooter);
	FHitResult FocusHit;
	const FVector FocusPoint = GetWorld()->LineTraceSingleByChannel(FocusHit, CameraStart, CameraEnd, ColorTransfer::ShotTraceChannel, Params) ? FocusHit.ImpactPoint : CameraEnd;

	const FVector Direction = (FocusPoint - ViewLocation).GetSafeNormal();
	return Direction.IsNearlyZero() ? CameraDirection : Direction;
}

void AColorShotBeam::UpdateLocalAim(const APawn* Shooter)
{
	AimDirection = ComputeLocalAimDirection(Shooter);

	if (HasAuthority() || (AimDirection | LastSentAimDirection) >= ColorShotBeam::AimResendDotThreshold)
	{
		return;
	}

	LastSentAimDirection = AimDirection;
	ServerSetAimDirection(AimDirection);
}

void AColorShotBeam::OnRep_Pigment()
{
	if (BeamMaterial)
	{
		BeamMaterial->SetVectorParameterValue(ColorShotBeam::ColorParam, ColorTransfer::GetPigmentColor(Pigment));
		BeamMaterial->SetScalarParameterValue(ColorShotBeam::EmissiveParam, Pigment == EColorPigment::Black ? 0.0f : 2.0f);
	}
}

void AColorShotBeam::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const APawn* Shooter = GetInstigator();
	if (!Shooter)
	{
		return;
	}

	if (Shooter->IsLocallyControlled())
	{
		UpdateLocalAim(Shooter);
	}

	TArray<FVector> Points;
	FHitResult Hit;
	const bool bHit = PredictPath(GetWorld(), const_cast<APawn*>(Shooter), Shooter->GetPawnViewLocation(), AimDirection, MaxRange, Points, Hit);
	UpdateSegments(Points);

	// 颜料为 None 时服务器不结算，也就没有水射出
	if (bHit && Pigment != EColorPigment::None)
	{
		TryPlayImpact(Hit);
	}
}

void AColorShotBeam::TryPlayImpact(const FHitResult& Hit)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastImpactTime < ImpactInterval)
	{
		return;
	}
	LastImpactTime = Now;

	// 与服务器结算的判定对象保持一致：机关或带颜料的角色算命中目标
	AActor* HitActor = Hit.GetActor();
	const bool bHitTarget = Cast<IColorShotReceiver>(HitActor) || UColorPigmentSet::FindForActor(HitActor);
	const float Intensity = bHitTarget ? 1.0f : SurfaceImpactScale;

	if (LoadedImpactEffect)
	{
		const FRotator Rotation = FRotationMatrix::MakeFromZ(Hit.ImpactNormal).Rotator();
		UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, LoadedImpactEffect, Hit.ImpactPoint, Rotation, FVector::OneVector, /*bAutoDestroy=*/true, /*bAutoActivate=*/false);
		if (Effect)
		{
			Effect->SetVariableLinearColor(ColorShotBeam::ImpactColorParam, ColorTransfer::GetPigmentColor(Pigment));
			Effect->SetVariableInt(ColorShotBeam::ImpactSpawnCountParam, FMath::Max(FMath::RoundToInt32(ImpactSpawnCount * Intensity), 1));
			Effect->Activate();
		}
	}

	if (LoadedImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LoadedImpactSound, Hit.ImpactPoint, Intensity);
	}
}

void AColorShotBeam::UpdateSegments(const TArray<FVector>& Points)
{
	const int32 NeededSegments = FMath::Max(Points.Num() - 1, 0);

	while (Segments.Num() < NeededSegments)
	{
		UStaticMeshComponent* Segment = NewObject<UStaticMeshComponent>(this);
		Segment->SetStaticMesh(SegmentMesh.LoadSynchronous());
		Segment->SetMaterial(0, BeamMaterial);
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Segment->SetCastShadow(false);
		Segment->SetupAttachment(RootComponent);
		Segment->SetUsingAbsoluteLocation(true);
		Segment->SetUsingAbsoluteRotation(true);
		Segment->SetUsingAbsoluteScale(true);
		Segment->RegisterComponent();
		Segments.Add(Segment);
	}

	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		UStaticMeshComponent* Segment = Segments[Index];
		if (Index >= NeededSegments)
		{
			Segment->SetVisibility(false);
			continue;
		}

		// 引擎圆柱沿 Z 轴、高 100、直径 100，按两端点拉伸
		const FVector From = Points[Index];
		const FVector To = Points[Index + 1];
		const FVector Delta = To - From;
		const float Scale = Thickness / 100.0f;

		Segment->SetWorldLocationAndRotation((From + To) * 0.5f, FRotationMatrix::MakeFromZ(Delta).Rotator());
		Segment->SetWorldScale3D(FVector(Scale, Scale, Delta.Size() / 100.0f));
		Segment->SetVisibility(true);
	}
}

void AColorShotBeam::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AColorShotBeam, Pigment);
	DOREPLIFETIME(AColorShotBeam, AimDirection);
	DOREPLIFETIME_CONDITION(AColorShotBeam, MaxRange, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AColorShotBeam, ImpactInterval, COND_InitialOnly);
}
