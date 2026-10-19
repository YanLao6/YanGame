#include "Actors/ColorAttractOrb.h"

#include "Actors/ColorFusionOrb.h"
#include "Attributes/ColorPigmentSet.h"
#include "Effects/ColorPigmentEffects.h"
#include "ColorTransferQueries.h"
#include "ColorTransferTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "MoverComponent.h"
#include "MoverLayeredMove/YanRadialAttractionMove.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "YanMoverAngelscriptLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorAttractOrb)

namespace ColorAttractOrb
{
	constexpr float InjectInterval = 1.0f;
}

AColorAttractOrb::AColorAttractOrb()
{
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AColorAttractOrb::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(InjectTimer, this, &ThisClass::AuthInjectTick, ColorAttractOrb::InjectInterval, /*bLoop=*/true);
		GetWorldTimerManager().SetTimer(AttractionTimer, this, &ThisClass::AuthRefreshAttraction, FMath::Max(AttractionRefreshInterval, 0.01f), /*bLoop=*/true);
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		LoadedDetonateEffect = DetonateEffect.LoadSynchronous();
		LoadedDetonateSound = DetonateSound.LoadSynchronous();
	}
}

void AColorAttractOrb::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);

	if (HasAuthority())
	{
		++AttractionUpdateSequence;
		for (const TWeakObjectPtr<UMoverComponent>& WeakMover : AttractedMovers)
		{
			if (UMoverComponent* Mover = WeakMover.Get())
			{
				AuthReleaseAttraction(Mover);
			}
		}
		AttractedMovers.Reset();
		OnControlFinished.Broadcast();
		OnControlFinished.Clear();
	}

	Super::EndPlay(EndPlayReason);
}

void AColorAttractOrb::AuthDetonate(const FVector& Location)
{
	MulticastDetonate(Location);
	Destroy();
}

void AColorAttractOrb::MulticastDetonate_Implementation(FVector_NetQuantize Location)
{
	// 专用服务器上 BeginPlay 不加载资源，两个指针均为空
	if (LoadedDetonateEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, LoadedDetonateEffect, Location);
	}

	if (LoadedDetonateSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LoadedDetonateSound, Location);
	}
}

void AColorAttractOrb::AuthLaunch(float Duration)
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	bControlled = true;
	SetLifeSpan(FMath::Max(Duration, UE_KINDA_SMALL_NUMBER));
}

void AColorAttractOrb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bControlled)
	{
		SendLocalControl();
	}

	if (!HasAuthority())
	{
		return;
	}

	if (bControlled)
	{
		AuthMoveTowardsAim(DeltaSeconds);
	}

	if (bControlled)
	{
		// 球脱手后碰到融合球的靶子即视为命中
		TArray<FOverlapResult> Overlaps;
		GetWorld()->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ColorTransfer::ShotTraceChannel, FCollisionShape::MakeSphere(TargetHitRadius));
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AColorFusionOrb::AuthNotifyChargeHit(Overlap.GetComponent());
		}
	}
}

void AColorAttractOrb::SendLocalControl()
{
	const APawn* Caster = GetInstigator();
	FVector CameraStart;
	FVector CameraDirection;
	if (!ColorTransfer::GetLocalCameraAim(Caster, /*bStartAtPawn=*/false, CameraStart, CameraDirection))
	{
		return;
	}

	const APlayerController* PC = CastChecked<APlayerController>(Caster->GetController());
	if (PC->WasInputKeyJustPressed(EKeys::LeftControl))
	{
		ServerCancelControl();
		return;
	}

	const int8 NewThrottle = static_cast<int8>((PC->IsInputKeyDown(EKeys::LeftMouseButton) ? 1 : 0) - (PC->IsInputKeyDown(EKeys::RightMouseButton) ? 1 : 0));
	ServerUpdateControl(CameraStart, CameraDirection, NewThrottle);
}

void AColorAttractOrb::ServerUpdateControl_Implementation(FVector_NetQuantize InAimStart, FVector_NetQuantizeNormal InAimDirection, int8 InThrottle)
{
	AimStart = InAimStart;
	AimDirection = InAimDirection.GetSafeNormal();
	Throttle = static_cast<int8>(FMath::Clamp<int32>(InThrottle, -1, 1));

	if (AimDistance < 0.0f)
	{
		AimDistance = FMath::Clamp((GetActorLocation() - AimStart) | AimDirection, AimDistanceRange.X, AimDistanceRange.Y);
	}
}

void AColorAttractOrb::ServerCancelControl_Implementation()
{
	// 销毁时 EndPlay 广播 OnControlFinished，由技能自行结束
	if (bControlled)
	{
		Destroy();
	}
}

void AColorAttractOrb::AuthMoveTowardsAim(float DeltaSeconds)
{
	// 尚未收到施法者的相机数据时原地悬停
	if (AimDistance < 0.0f || DeltaSeconds <= 0.0f)
	{
		return;
	}

	AimDistance = FMath::Clamp(AimDistance + Throttle * PushSpeed * DeltaSeconds, AimDistanceRange.X, AimDistanceRange.Y);

	const FVector OldLocation = GetActorLocation();
	const FVector NewLocation = FMath::VInterpConstantTo(OldLocation, AimStart + AimDirection * AimDistance, DeltaSeconds, MoveSpeed);
	SetActorLocation(NewLocation);
}

float AColorAttractOrb::GetAttractRadius() const
{
	return FMath::Lerp(AttractRadiusRange.X, AttractRadiusRange.Y, GetChargeAlpha());
}

TSet<APawn*> AColorAttractOrb::GatherAttractedPawns() const
{
	TSet<APawn*> Pawns = ColorTransfer::GatherPawnsInShape(GetWorld(), GetActorLocation(), FQuat::Identity, FCollisionShape::MakeSphere(GetAttractRadius()));
	Pawns.Remove(GetInstigator());
	return Pawns;
}

void AColorAttractOrb::AuthRefreshAttraction()
{
	const FVector OrbLocation = GetActorLocation();
	const double SampleTime = GetWorld()->GetTimeSeconds();
	const double SampleDelta = SampleTime - LastAttractionSampleTime;
	const FVector OrbVelocity = LastAttractionSampleTime >= 0.0 && SampleDelta > UE_KINDA_SMALL_NUMBER ? (OrbLocation - LastAttractionLocation) / SampleDelta : FVector::ZeroVector;
	LastAttractionLocation = OrbLocation;
	LastAttractionSampleTime = SampleTime;
	++AttractionUpdateSequence;

	FYanLayeredMove_RadialAttraction Settings;
	Settings.SourceId = static_cast<int32>(GetUniqueID());
	Settings.UpdateSequence = AttractionUpdateSequence;
	Settings.Location = OrbLocation;
	Settings.CenterVelocity = OrbVelocity;
	Settings.Radius = GetAttractRadius();
	Settings.Magnitude = FMath::Max(PullSpeed, 0.0f);
	Settings.bCaptureToCenter = true;
	Settings.MixMode = EMoveMixMode::OverrideVelocity;
	Settings.InnerRadius = FMath::Clamp(InnerRadius, 0.0f, Settings.Radius);
	Settings.EntryVelocityDamping = FMath::Clamp(EntryVelocityDamping, 0.0f, 1.0f);
	Settings.CaptureDurationMs = FMath::Max(InnerBlinkDurationMs, 1.0f);
	Settings.CaptureEaseExponent = FMath::Max(InnerBlinkEaseExponent, 0.01f);
	// 未捕获时用租期覆盖刷新抖动；捕获后由模拟切换为持续到显式取消。
	Settings.DurationMs = FMath::Max(AttractionRefreshInterval, 0.01f) * 3000.0f;

	for (APawn* Pawn : GatherAttractedPawns())
	{
		if (UMoverComponent* Mover = Pawn->FindComponentByClass<UMoverComponent>())
		{
			AttractedMovers.Add(Mover);
		}
	}

	// 捕获在异步模拟中发生，范围扫描不能判定目标已脱离；已接触目标持续接收中心快照。
	for (auto It = AttractedMovers.CreateIterator(); It; ++It)
	{
		if (UMoverComponent* Mover = It->Get())
		{
			UYanMoverAngelscriptLibrary::RefreshRadialAttractionToTarget(Mover, Settings);
		}
		else
		{
			It.RemoveCurrent();
		}
	}
}

void AColorAttractOrb::AuthReleaseAttraction(UMoverComponent* Mover)
{
	FYanLayeredMove_RadialAttraction Settings;
	Settings.SourceId = static_cast<int32>(GetUniqueID());
	Settings.UpdateSequence = AttractionUpdateSequence;
	Settings.Radius = 0.0f;
	UYanMoverAngelscriptLibrary::RefreshRadialAttractionToTarget(Mover, Settings);
}

void AColorAttractOrb::AuthInjectTick()
{
	const float Amount = FMath::Lerp(PigmentPerSecondRange.X, PigmentPerSecondRange.Y, GetChargeAlpha()) * ColorAttractOrb::InjectInterval;

	for (APawn* Pawn : GatherAttractedPawns())
	{
		if (UColorPigmentSet* PigmentSet = UColorPigmentSet::FindForActor(Pawn))
		{
			ColorTransfer::AuthApplyInjection(*PigmentSet, EColorPigment::Blue, Amount, this);
		}
	}
}

void AColorAttractOrb::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AColorAttractOrb, bControlled);
}
