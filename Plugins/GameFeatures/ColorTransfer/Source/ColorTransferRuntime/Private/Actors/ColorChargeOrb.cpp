#include "Actors/ColorChargeOrb.h"

#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorChargeOrb)

AColorChargeOrb::AColorChargeOrb()
{
	OrbMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrbMesh"));
	OrbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OrbMesh->SetCastShadow(false);
	RootComponent = OrbMesh;

	SetNetUpdateFrequency(60.0f);

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AColorChargeOrb::BeginPlay()
{
	Super::BeginPlay();

	OnRep_ChargeAlpha();
}

void AColorChargeOrb::AuthSetCharge(float Alpha, int32 Tier)
{
	ChargeAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	OnRep_ChargeAlpha();

	const uint8 NewTier = static_cast<uint8>(FMath::Clamp(Tier, 0, 255));
	if (NewTier != ChargeTier)
	{
		ChargeTier = NewTier;
		OnRep_ChargeTier();
	}
}

void AColorChargeOrb::AuthFlyTo(const FVector& TargetLocation, USceneComponent* TargetComponent, FName TargetSocket)
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	FlyTargetLocation = TargetLocation;
	FlyTargetComponent = TargetComponent;
	FlyTargetSocket = TargetSocket;
	bFlying = true;
	SetActorTickEnabled(true);
}

FVector AColorChargeOrb::GetFlyTarget() const
{
	const USceneComponent* TargetComponent = FlyTargetComponent.Get();
	return TargetComponent ? TargetComponent->GetSocketLocation(FlyTargetSocket) : FlyTargetLocation;
}

void AColorChargeOrb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// bFlying 只由服务器的 AuthFlyTo 置位
	if (!bFlying)
	{
		return;
	}

	const FVector Target = GetFlyTarget();
	const FVector NewLocation = FMath::VInterpConstantTo(GetActorLocation(), Target, DeltaSeconds, FlySpeed);
	SetActorLocation(NewLocation);

	if (!NewLocation.Equals(Target, 1.0f))
	{
		return;
	}

	bFlying = false;
	if (!OnArrived.ExecuteIfBound())
	{
		Destroy();
	}
}

void AColorChargeOrb::OnRep_ChargeAlpha()
{
	OrbMesh->SetRelativeScale3D(FVector(FMath::Lerp(ScaleRange.X, ScaleRange.Y, ChargeAlpha)));
}

void AColorChargeOrb::OnRep_ChargeTier()
{
	if (GetNetMode() == NM_DedicatedServer || TierSounds.IsEmpty())
	{
		return;
	}

	if (USoundBase* Sound = TierSounds[FMath::Min<int32>(ChargeTier, TierSounds.Num() - 1)])
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
}

void AColorChargeOrb::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AColorChargeOrb, ChargeAlpha);
	DOREPLIFETIME(AColorChargeOrb, ChargeTier);
}
