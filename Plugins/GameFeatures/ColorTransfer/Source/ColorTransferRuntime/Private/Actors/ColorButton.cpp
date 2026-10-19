#include "Actors/ColorButton.h"

#include "Actors/ColorMovingFloor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorButton)

namespace ColorButton
{
	const FName ColorParam(TEXT("Color"));

	// 射线每次结算的量不是整数，总量差这点也视为池满
	constexpr float FullPoolTolerance = 0.5f;

	constexpr float ColorSettleTolerance = 0.002f;

	float GetPigmentShare(const TArray<FColorPigmentLayer>& Layers, EColorPigment Pigment, float Total)
	{
		float Amount = 0.0f;
		for (const FColorPigmentLayer& Layer : Layers)
		{
			if (Layer.Pigment == Pigment)
			{
				Amount += Layer.Amount;
			}
		}

		return Amount / Total;
	}
}

AColorButton::AColorButton()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	bReplicates = true;

	// 只在显示色渐变期间 Tick
	PrimaryActorTick.bCanEverTick          = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AColorButton::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		AuthResetPool();
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		// 提前加载并持有，避免在碎裂那一帧同步加载卡顿
		LoadedExplosionEffect = ExplosionEffect.LoadSynchronous();

		// 渐变起点：开局颜料色，空池则取材质原有颜色
		if (DisplayPigment != EColorPigment::None)
		{
			DisplayedColor = ColorTransfer::GetPigmentColor(DisplayPigment);
			Mesh->SetVectorParameterValueOnMaterials(ColorButton::ColorParam, FVector(DisplayedColor));
		}
		else if (const UMaterialInterface* Material = Mesh->GetMaterial(0))
		{
			FLinearColor MaterialColor;
			if (Material->GetVectorParameterValue(FHashedMaterialParameterInfo(ColorButton::ColorParam), MaterialColor))
			{
				DisplayedColor = FLinearColor(MaterialColor.R, MaterialColor.G, MaterialColor.B, 1.0f);
			}
		}

		InitialDisplayedColor = DisplayedColor;
		OnRep_PoolColor();
	}
}

void AColorButton::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);

	Super::EndPlay(EndPlayReason);
}

void AColorButton::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	DisplayedColor = FMath::CInterpTo(DisplayedColor, PoolColor, DeltaSeconds, ColorInterpSpeed);
	if (DisplayedColor.Equals(PoolColor, ColorButton::ColorSettleTolerance))
	{
		DisplayedColor = PoolColor;
		SetActorTickEnabled(false);
	}

	Mesh->SetVectorParameterValueOnMaterials(ColorButton::ColorParam, FVector(DisplayedColor));
}

void AColorButton::ReceiveColorShot(const FColorShotHit& Shot)
{
	if (bPrimed || Shot.Pigment == EColorPigment::None || Shot.Amount <= 0.0f)
	{
		return;
	}

	AuthInjectPigment(Shot.Pigment, Shot.Amount);

	if (!AuthShouldPrime(Shot))
	{
		return;
	}

	bPrimed = true;
	OnRep_Visual();

	GetWorldTimerManager().SetTimer(FuseTimer, this, &ThisClass::AuthBreak, FMath::Max(FuseDelay, UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);
}

bool AColorButton::AuthShouldPrime(const FColorShotHit& Shot)
{
	return IsPoolMatchingTarget();
}

void AColorButton::AuthInjectPigment(EColorPigment Pigment, float Amount)
{
	if (FColorPigmentLayer* Existing = PoolContents.FindByPredicate([Pigment](const FColorPigmentLayer& Layer) { return Layer.Pigment == Pigment; }))
	{
		Existing->Amount += Amount;
	}
	else
	{
		PoolContents.Add({Pigment, Amount});
	}

	// 溢出的是已混匀的颜料，各颜料按同一比例减少
	const float Total = ColorTransfer::GetLayersTotal(PoolContents);
	if (Total > PoolCapacity)
	{
		const float Scale = PoolCapacity / Total;
		for (FColorPigmentLayer& Layer : PoolContents)
		{
			Layer.Amount *= Scale;
		}
	}

	PoolColor = ColorTransfer::MixPigments(PoolContents);
	OnRep_PoolColor();
}

bool AColorButton::IsPoolMatchingTarget() const
{
	const float PoolTotal   = ColorTransfer::GetLayersTotal(PoolContents);
	const float TargetTotal = ColorTransfer::GetLayersTotal(TargetMix);
	if (PoolTotal < PoolCapacity - ColorButton::FullPoolTolerance || TargetTotal <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	// 双向比对：池里多出的颜料与目标里缺少的颜料都算偏差
	auto IsShareMatching = [&](EColorPigment Pigment)
	{
		const float PoolShare   = ColorButton::GetPigmentShare(PoolContents, Pigment, PoolTotal);
		const float TargetShare = ColorButton::GetPigmentShare(TargetMix, Pigment, TargetTotal);
		return FMath::Abs(PoolShare - TargetShare) <= MixTolerance;
	};

	for (const FColorPigmentLayer& Layer : PoolContents)
	{
		if (!IsShareMatching(Layer.Pigment))
		{
			return false;
		}
	}

	for (const FColorPigmentLayer& Layer : TargetMix)
	{
		if (!IsShareMatching(Layer.Pigment))
		{
			return false;
		}
	}

	return true;
}

void AColorButton::AuthBreak()
{
	bBroken = true;
	OnRep_Visual();
	MulticastExplode(GetActorLocation());

	// 先于 AuthOnBroken 设置：子类在其中销毁自身时计时器随 EndPlay 一并清除
	if (RespawnDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(RespawnTimer, this, &ThisClass::AuthRespawn, RespawnDelay, /*bLoop=*/false);
	}

	AuthOnBroken();
}

void AColorButton::AuthRespawn()
{
	AuthResetPool();

	bPrimed = false;
	bBroken = false;
	OnRep_Visual();
	OnRep_PoolColor();
}

void AColorButton::AuthResetPool()
{
	PoolContents.Reset();
	if (DisplayPigment != EColorPigment::None)
	{
		PoolContents.Add({DisplayPigment, PoolCapacity});
	}

	PoolColor = PoolContents.IsEmpty() ? FLinearColor::Transparent : ColorTransfer::MixPigments(PoolContents);
}

void AColorButton::AuthOnBroken()
{
	if (TriggeredFloor)
	{
		TriggeredFloor->AuthStartMoving();
	}
}

void AColorButton::MulticastExplode_Implementation(FVector_NetQuantize Location)
{
	if (GetNetMode() != NM_DedicatedServer && LoadedExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, LoadedExplosionEffect, Location);
	}
}

void AColorButton::OnRep_Visual()
{
	// 重生时颜色直接回到开局状态，不从碎裂前的混合色渐变
	if (!bBroken && !Mesh->IsVisible() && GetNetMode() != NM_DedicatedServer)
	{
		DisplayedColor = PoolColor.A > 0.0f ? PoolColor : InitialDisplayedColor;
		if (DisplayedColor.A > 0.0f)
		{
			Mesh->SetVectorParameterValueOnMaterials(ColorButton::ColorParam, FVector(DisplayedColor));
		}
		SetActorTickEnabled(false);
	}

	Mesh->SetVisibility(!bBroken);
	Mesh->SetCollisionEnabled(bBroken ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
}

void AColorButton::OnRep_PoolColor()
{
	if (GetNetMode() == NM_DedicatedServer || PoolColor.A <= 0.0f)
	{
		return;
	}

	// 材质没有可读的起点色时直接跳到混合色
	if (DisplayedColor.A <= 0.0f)
	{
		DisplayedColor = PoolColor;
		Mesh->SetVectorParameterValueOnMaterials(ColorButton::ColorParam, FVector(DisplayedColor));
		return;
	}

	SetActorTickEnabled(true);
}

void AColorButton::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AColorButton, bPrimed);
	DOREPLIFETIME(AColorButton, bBroken);
	DOREPLIFETIME(AColorButton, PoolColor);
}
