#include "Actors/ColorAttractOrb.h"

#include "Actors/ColorFusionOrb.h"
#include "Attributes/ColorPigmentSet.h"
#include "Effects/ColorPigmentEffects.h"
#include "ColorTransferTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

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
		StartAttraction();
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		LoadedDetonateEffect = DetonateEffect.LoadSynchronous();
		LoadedDetonateSound  = DetonateSound.LoadSynchronous();
	}
}

void AColorAttractOrb::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(InjectTimer);
	StopAttraction();

	if (HasAuthority())
	{
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
	if (!HasAuthority())
	{
		return;
	}

	StartControl();
	SetLifeSpan(FMath::Max(Duration, UE_KINDA_SMALL_NUMBER));
}

void AColorAttractOrb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !IsControlled() || IsActorBeingDestroyed())
	{
		return;
	}

	// 球脱手后碰到融合球的靶子即视为命中
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ColorTransfer::ShotTraceChannel, FCollisionShape::MakeSphere(TargetHitRadius));
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AColorFusionOrb::AuthNotifyChargeHit(Overlap.GetComponent());
	}
}

void AColorAttractOrb::SendLocalControl()
{
	const APawn*             Caster = GetInstigator();
	const APlayerController* PC     = Caster ? Cast<APlayerController>(Caster->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	if (PC->WasInputKeyJustPressed(EKeys::LeftControl))
	{
		ServerCancelControl();
		return;
	}

	const int8 NewThrottle = static_cast<int8>((PC->IsInputKeyDown(EKeys::LeftMouseButton) ? 1 : 0) - (PC->IsInputKeyDown(EKeys::RightMouseButton) ? 1 : 0));
	SubmitControlInput(NewThrottle);
}

void AColorAttractOrb::ServerCancelControl_Implementation()
{
	// 销毁时 EndPlay 广播 OnControlFinished，由技能自行结束
	if (IsControlled())
	{
		Destroy();
	}
}

float AColorAttractOrb::GetAttractRadius_Implementation() const
{
	return FMath::Lerp(AttractRadiusRange.X, AttractRadiusRange.Y, GetChargeAlpha());
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
