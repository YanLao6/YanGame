#include "Actors/ColorFusionOrb.h"

#include "ColorTransferRuntimeModule.h"
#include "ColorTransferTravel.h"
#include "Components/StaticMeshComponent.h"
#include "ColorTransferQueries.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorFusionOrb)

namespace ColorFusionOrb
{
	// 射线每次结算的量不是整数，存量允许这点误差即视为注满
	constexpr float FillTolerance = 0.5f;

	const FName ColorParam(TEXT("Color"));
	const FName EmissiveParam(TEXT("EmissiveStrength"));
}

AColorFusionOrb::AColorFusionOrb()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	// 只在闪烁与融合期间需要逐帧更新，由 OnRep_Visual 按状态开关
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AColorFusionOrb::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	TInlineComponentArray<UStaticMeshComponent*> MeshComponents(this);
	for (UStaticMeshComponent* Mesh : MeshComponents)
	{
		if (Mesh->GetFName() == OrbMeshName)
		{
			OrbMesh = Mesh;
		}
		else if (Mesh->GetFName() == TargetMeshName)
		{
			TargetMesh = Mesh;
		}
	}

	UE_CLOG(!OrbMesh || !TargetMesh, LogColorTransfer, Error, TEXT("%s 找不到网格组件 %s / %s"), *GetName(), *OrbMeshName.ToString(), *TargetMeshName.ToString());
}

void AColorFusionOrb::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() != NM_DedicatedServer)
	{
		if (OrbMesh)
		{
			OrbMaterial = OrbMesh->CreateDynamicMaterialInstance(0);
		}

		// 提前加载并持有，避免在爆炸那一帧同步加载卡顿
		LoadedExplosionEffect = ExplosionEffect.LoadSynchronous();
		OnRep_Visual();
	}
}

void AColorFusionOrb::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);

	Super::EndPlay(EndPlayReason);
}

bool AColorFusionOrb::AuthNotifyChargeHit(const UPrimitiveComponent* Component)
{
	AColorFusionOrb* Orb = Component ? Cast<AColorFusionOrb>(Component->GetOwner()) : nullptr;
	if (!Orb || Component != Orb->TargetMesh || !Orb->HasAuthority())
	{
		return false;
	}

	AColorFusionOrb* Other = Orb->Partner;
	if (Other && Orb->IsArmed() && Other->IsArmed())
	{
		const FVector MeetLocation = (Orb->GetActorLocation() + Other->GetActorLocation()) * 0.5f;
		Orb->AuthStartFusion(MeetLocation);
		Other->AuthStartFusion(MeetLocation);

		// 由被命中的一方统一引爆并切关
		FTimerHandle ExplodeTimer;
		Orb->GetWorldTimerManager().SetTimer(ExplodeTimer, Orb, &ThisClass::AuthExplode, Orb->FusionMoveDuration + Orb->PurpleDuration, false);
	}

	return true;
}

void AColorFusionOrb::ReceiveColorShot(const FColorShotHit& Shot)
{
	if (Shot.Pigment != AcceptedPigment || bArmed)
	{
		return;
	}

	StoredAmount = FMath::Min(StoredAmount + Shot.Amount, Capacity);
	if (StoredAmount >= Capacity - ColorFusionOrb::FillTolerance)
	{
		AuthArm();
	}

	OnRep_Visual();
}

void AColorFusionOrb::AuthArm()
{
	StoredAmount = Capacity;
	bArmed = true;
	GetWorldTimerManager().SetTimer(ArmedTimer, this, &ThisClass::AuthDisarm, ArmedDuration, false);
}

void AColorFusionOrb::AuthDisarm()
{
	StoredAmount = 0.0f;
	bArmed = false;
	OnRep_Visual();
}

void AColorFusionOrb::AuthStartFusion(const FVector& MeetLocation)
{
	GetWorldTimerManager().ClearTimer(ArmedTimer);

	FusionFrom = GetActorLocation();
	FusionTo = MeetLocation;
	FusionStartTime = ColorTransfer::GetServerTime(GetWorld());
	OnRep_Visual();
}

void AColorFusionOrb::AuthExplode()
{
	MulticastExplode(FusionTo);

	const TSoftObjectPtr<UWorld> Destination = NextLevel.IsNull() && Partner ? Partner->NextLevel : NextLevel;

	ColorTransfer::ServerTravelToLevel(this, Destination);

	if (Partner)
	{
		Partner->Destroy();
	}

	Destroy();
}

void AColorFusionOrb::MulticastExplode_Implementation(FVector_NetQuantize Location)
{
	if (GetNetMode() != NM_DedicatedServer && LoadedExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, LoadedExplosionEffect, Location);
	}
}

void AColorFusionOrb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const double Now = ColorTransfer::GetServerTime(GetWorld());

	if (IsFusing())
	{
		// 加速靠拢，模拟相互吸引撞到一起
		const float MoveAlpha = FMath::Clamp(static_cast<float>((Now - FusionStartTime) / FusionMoveDuration), 0.0f, 1.0f);
		SetActorLocation(FMath::Lerp(FVector(FusionFrom), FVector(FusionTo), MoveAlpha * MoveAlpha));

		if (OrbMaterial)
		{
			const bool bCollided = MoveAlpha >= 1.0f;
			OrbMaterial->SetVectorParameterValue(ColorFusionOrb::ColorParam, bCollided ? ColorTransfer::PurpleColor : ArmedColor);
			OrbMaterial->SetScalarParameterValue(ColorFusionOrb::EmissiveParam, bCollided ? 6.0f : 2.0f);
		}
	}
	else if (OrbMaterial)
	{
		const float Blink = 0.5f + 0.5f * FMath::Sin(static_cast<float>(Now) * BlinkFrequency * UE_TWO_PI);
		OrbMaterial->SetScalarParameterValue(ColorFusionOrb::EmissiveParam, FMath::Lerp(0.2f, 5.0f, Blink));
	}
}

void AColorFusionOrb::OnRep_Visual()
{
	// 服务器也要逐帧推动融合位移，客户端另需闪烁
	SetActorTickEnabled(bArmed);

	if (!OrbMaterial)
	{
		return;
	}

	OrbMaterial->SetVectorParameterValue(ColorFusionOrb::ColorParam, bArmed ? ArmedColor : IdleColor);
	// 未满时存量越多越亮，让玩家看出注了多少
	OrbMaterial->SetScalarParameterValue(ColorFusionOrb::EmissiveParam, 0.3f + 1.5f * StoredAmount / Capacity);
}

void AColorFusionOrb::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AColorFusionOrb, StoredAmount);
	DOREPLIFETIME(AColorFusionOrb, bArmed);
	DOREPLIFETIME(AColorFusionOrb, FusionStartTime);
	DOREPLIFETIME(AColorFusionOrb, FusionFrom);
	DOREPLIFETIME(AColorFusionOrb, FusionTo);
}
