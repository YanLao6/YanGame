#include "Actors/ColorCloverDoor.h"

#include "ColorTransferTravel.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorCloverDoor)

namespace ColorCloverDoor
{
	// 左上、右上、左下、右下
	constexpr EColorPigment LeafPigments[ColorCloverLeafCount] = {EColorPigment::White, EColorPigment::Blue, EColorPigment::Red, EColorPigment::Black};

	const FName LeafColorParams[ColorCloverLeafCount] = {TEXT("LeafColor0"), TEXT("LeafColor1"), TEXT("LeafColor2"), TEXT("LeafColor3")};
	const FName LeafFillParams[ColorCloverLeafCount] = {TEXT("LeafFill0"), TEXT("LeafFill1"), TEXT("LeafFill2"), TEXT("LeafFill3")};

	// 染满后留给玩家看清门被点亮的时间
	constexpr float TravelDelay = 1.5f;
}

AColorCloverDoor::AColorCloverDoor()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(Root);

	FaceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FaceMesh"));
	FaceMesh->SetupAttachment(Root);
	FaceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	bReplicates = true;
}

void AColorCloverDoor::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() != NM_DedicatedServer)
	{
		FaceMaterial = FaceMesh->CreateDynamicMaterialInstance(0);
		if (FaceMaterial)
		{
			for (int32 LeafIndex = 0; LeafIndex < ColorCloverLeafCount; ++LeafIndex)
			{
				FaceMaterial->SetVectorParameterValue(ColorCloverDoor::LeafColorParams[LeafIndex], ColorTransfer::GetPigmentColor(ColorCloverDoor::LeafPigments[LeafIndex]));
			}
		}

		OnRep_LeafProgress();
	}
}

bool AColorCloverDoor::IsFilled() const
{
	for (const float Progress : LeafProgress)
	{
		if (Progress < RequiredPerLeaf)
		{
			return false;
		}
	}

	return true;
}

void AColorCloverDoor::ReceiveColorShot(const FColorShotHit& Shot)
{
	if (IsFilled())
	{
		return;
	}

	const FVector LocalHit = GetActorTransform().InverseTransformPosition(Shot.Hit.ImpactPoint);
	const bool bLeftSide = LocalHit.Y > 0.0f;
	const bool bTopSide = LocalHit.Z > 0.0f;
	const int32 LeafIndex = (bTopSide ? 0 : 2) + (bLeftSide ? 0 : 1);

	if (Shot.Pigment != ColorCloverDoor::LeafPigments[LeafIndex])
	{
		return;
	}

	LeafProgress[LeafIndex] = FMath::Min(LeafProgress[LeafIndex] + Shot.Amount, RequiredPerLeaf);
	OnRep_LeafProgress();

	if (IsFilled())
	{
		if (NextLevel.IsNull())
		{
			Destroy();
			return;
		}

		GetWorldTimerManager().SetTimer(TravelTimer, this, &ThisClass::TravelToNextLevel, ColorCloverDoor::TravelDelay, /*bLoop=*/false);
	}
}

void AColorCloverDoor::OnRep_LeafProgress()
{
	if (FaceMaterial)
	{
		for (int32 LeafIndex = 0; LeafIndex < ColorCloverLeafCount; ++LeafIndex)
		{
			FaceMaterial->SetScalarParameterValue(ColorCloverDoor::LeafFillParams[LeafIndex], LeafProgress[LeafIndex] / RequiredPerLeaf);
		}
	}
}

void AColorCloverDoor::TravelToNextLevel()
{
	ColorTransfer::ServerTravelToLevel(this, NextLevel);
}

void AColorCloverDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AColorCloverDoor, LeafProgress);
}
