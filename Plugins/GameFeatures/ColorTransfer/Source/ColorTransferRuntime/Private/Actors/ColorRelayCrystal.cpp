#include "Actors/ColorRelayCrystal.h"

#include "Actors/ColorWindZone.h"
#include "Attributes/ColorPigmentSet.h"
#include "Effects/ColorPigmentEffects.h"
#include "ColorTransferQueries.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorRelayCrystal)

namespace ColorRelayCrystal
{
	constexpr float AbsorbInterval = 0.2f;

	// 射线每次结算的量不是整数，存量允许这点误差即视为达标
	constexpr float RequirementTolerance = 0.5f;

	const FName ColorParam(TEXT("Color"));
	const FName EmissiveParam(TEXT("EmissiveStrength"));

	const FLinearColor EmptyColor(0.3f, 0.3f, 0.3f);
}

AColorRelayCrystal::AColorRelayCrystal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HintMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HintMesh"));
	HintMesh->SetupAttachment(GetStaticMeshComponent());

	AbsorbVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("AbsorbVolume"));
	AbsorbVolume->SetupAttachment(GetStaticMeshComponent());
	AbsorbVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AbsorbVolume->SetGenerateOverlapEvents(false);

	bReplicates = true;
	SetReplicatingMovement(false);
}

void AColorRelayCrystal::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() != NM_DedicatedServer)
	{
		HintMaterial = HintMesh->CreateDynamicMaterialInstance(0);
		// 提前加载并持有，避免在爆炸那一帧同步加载卡顿
		LoadedExplosionEffect = ExplosionEffect.LoadSynchronous();
		OnRep_Visual();
	}

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(AbsorbTimer, this, &ThisClass::AuthAbsorbTick, ColorRelayCrystal::AbsorbInterval, /*bLoop=*/true);
	}
}

void AColorRelayCrystal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);

	Super::EndPlay(EndPlayReason);
}

float AColorRelayCrystal::GetStoredAmount(EColorPigment Pigment) const
{
	float Amount = 0.0f;
	for (const FColorPigmentLayer& Layer : StoredLayers)
	{
		if (Layer.Pigment == Pigment)
		{
			Amount += Layer.Amount;
		}
	}

	return Amount;
}

float AColorRelayCrystal::GetTotalStoredAmount() const
{
	return ColorTransfer::GetLayersTotal(StoredLayers);
}

bool AColorRelayCrystal::AcceptsPigment(EColorPigment Pigment) const
{
	return Pigment != EColorPigment::None && (AcceptedPigments.IsEmpty() || AcceptedPigments.Contains(Pigment));
}

void AColorRelayCrystal::ReceiveColorShot(const FColorShotHit& Shot)
{
	if (bCompleted || !AcceptsPigment(Shot.Pigment))
	{
		return;
	}

	// 超出容量的部分直接损失，射手照样被扣除
	const float Accepted = FMath::Min(Shot.Amount, Capacity - GetTotalStoredAmount());
	if (Accepted <= 0.0f)
	{
		return;
	}

	ColorTransfer::PushLayer(StoredLayers, Shot.Pigment, Accepted);

	OnRep_Visual();
	AuthCheckCompletion();
}

void AColorRelayCrystal::AuthAbsorbTick()
{
	if (bCompleted || StoredLayers.IsEmpty())
	{
		return;
	}

	bool bChanged = false;
	for (APawn* Pawn : ColorTransfer::GatherPawnsInVolume(AbsorbVolume))
	{
		if (StoredLayers.IsEmpty())
		{
			break;
		}

		UColorPigmentSet* PigmentSet = UColorPigmentSet::FindForActor(Pawn);
		if (!PigmentSet)
		{
			continue;
		}

		// 只按角色实际吸收的量扣除：角色自身颜料耗尽或同色冲刷不动时，晶体里的颜料留着
		const FColorPigmentLayer& Top = StoredLayers.Last();
		const float Absorbed = ColorTransfer::AuthApplyInjection(*PigmentSet, Top.Pigment, FMath::Min(Top.Amount, AbsorbPerSecond * ColorRelayCrystal::AbsorbInterval), this);
		if (Absorbed > 0.0f)
		{
			ColorTransfer::DrainTopLayer(StoredLayers, Absorbed);
			bChanged = true;
		}
	}

	if (bChanged)
	{
		OnRep_Visual();
		AuthCheckCompletion();
	}
}

void AColorRelayCrystal::AuthCheckCompletion()
{
	if (bCompleted || RequiredAmounts.IsEmpty())
	{
		return;
	}

	for (const TPair<EColorPigment, float>& Requirement : RequiredAmounts)
	{
		if (GetStoredAmount(Requirement.Key) < Requirement.Value - ColorRelayCrystal::RequirementTolerance)
		{
			return;
		}
	}

	bCompleted = true;
	OnRep_Visual();

	for (AColorWindZone* Wind : WindsToDeactivate)
	{
		if (Wind)
		{
			Wind->AuthDeactivate();
		}
	}

	if (bExplodeOnComplete)
	{
		GetWorldTimerManager().SetTimer(ExplodeTimer, this, &ThisClass::AuthExplode, FMath::Max(ExplodeDelay, UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);
	}
}

void AColorRelayCrystal::AuthExplode()
{
	MulticastExplode(GetActorLocation());
	Destroy();
}

void AColorRelayCrystal::MulticastExplode_Implementation(FVector_NetQuantize Location)
{
	if (GetNetMode() != NM_DedicatedServer && LoadedExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, LoadedExplosionEffect, Location);
	}
}

void AColorRelayCrystal::OnRep_Visual()
{
	if (!HintMaterial)
	{
		return;
	}

	FLinearColor Color = ColorRelayCrystal::EmptyColor;
	if (bCompleted && bExplodeOnComplete)
	{
		Color = ColorTransfer::PurpleColor;
	}
	else if (AcceptedPigments.Num() == 1)
	{
		Color = ColorTransfer::GetPigmentColor(AcceptedPigments[0]);
	}
	else if (!StoredLayers.IsEmpty())
	{
		Color = ColorTransfer::GetPigmentColor(StoredLayers.Last().Pigment);
	}

	HintMaterial->SetVectorParameterValue(ColorRelayCrystal::ColorParam, Color);
	// 存量越多越亮，让玩家远处也能看出装了多少
	HintMaterial->SetScalarParameterValue(ColorRelayCrystal::EmissiveParam, 0.5f + 4.0f * GetTotalStoredAmount() / Capacity);
}

void AColorRelayCrystal::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AColorRelayCrystal, StoredLayers);
	DOREPLIFETIME(AColorRelayCrystal, bCompleted);
}
