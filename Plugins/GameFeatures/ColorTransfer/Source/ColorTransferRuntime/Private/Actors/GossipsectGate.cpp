#include "Actors/GossipsectGate.h"

#include "ColorTransferTravel.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ModularSystemStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GossipsectGate)

namespace GossipsectGate
{
	const FName BlackFillParam(TEXT("BlackFill"));
	const FName WhiteFillParam(TEXT("WhiteFill"));
	const FName PortalOpenParam(TEXT("PortalOpen"));

	constexpr float PortalCheckInterval = 0.1f;
}

AGossipsectGate::AGossipsectGate()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(Root);

	FaceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FaceMesh"));
	FaceMesh->SetupAttachment(Root);
	FaceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PortalVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("PortalVolume"));
	PortalVolume->SetupAttachment(Root);
	PortalVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PortalVolume->SetRelativeLocation(FVector(100.0f, 0.0f, 0.0f));
	PortalVolume->SetBoxExtent(FVector(60.0f, 180.0f, 180.0f));

	bReplicates = true;
}

void AGossipsectGate::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() != NM_DedicatedServer)
	{
		FaceMaterial = FaceMesh->CreateDynamicMaterialInstance(0);
		OnRep_DyeProgress();
	}
}

bool AGossipsectGate::IsOpen() const
{
	return BlackProgress >= RequiredPerHalf && WhiteProgress >= RequiredPerHalf;
}

void AGossipsectGate::ReceiveColorShot(const FColorShotHit& Shot)
{
	if (IsOpen())
	{
		return;
	}

	const FVector LocalHit = GetActorTransform().InverseTransformPosition(Shot.Hit.ImpactPoint);
	const bool bHitBlackHalf = LocalHit.Y < 0.0f;

	if (bHitBlackHalf && Shot.Pigment == EColorPigment::Black)
	{
		BlackProgress = FMath::Min(BlackProgress + Shot.Amount, RequiredPerHalf);
	}
	else if (!bHitBlackHalf && Shot.Pigment == EColorPigment::White)
	{
		WhiteProgress = FMath::Min(WhiteProgress + Shot.Amount, RequiredPerHalf);
	}
	else
	{
		return;
	}

	OnRep_DyeProgress();

	if (!IsOpen())
	{
		return;
	}

	if (bDestroyWhenOpened)
	{
		Destroy();
		return;
	}

	GetWorldTimerManager().SetTimer(PortalTimer, this, &ThisClass::EvaluatePortal, GossipsectGate::PortalCheckInterval, /*bLoop=*/true);
}

void AGossipsectGate::OnRep_DyeProgress()
{
	if (FaceMaterial)
	{
		FaceMaterial->SetScalarParameterValue(GossipsectGate::BlackFillParam, BlackProgress / RequiredPerHalf);
		FaceMaterial->SetScalarParameterValue(GossipsectGate::WhiteFillParam, WhiteProgress / RequiredPerHalf);
		FaceMaterial->SetScalarParameterValue(GossipsectGate::PortalOpenParam, IsOpen() ? 1.0f : 0.0f);
	}
}

void AGossipsectGate::EvaluatePortal()
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!GameState || bTravelStarted)
	{
		return;
	}

	int32 ActivePlayers = 0;
	int32 PlayersInPortal = 0;

	// PlayerArray 是 TObjectPtr 数组，迭代器产出 const 引用，需按下标取用
	for (int32 PlayerIndex = 0; PlayerIndex < GameState->PlayerArray.Num(); ++PlayerIndex)
	{
		const APlayerState* PlayerState = GameState->PlayerArray[PlayerIndex];
		if (!PlayerState || PlayerState->IsOnlyASpectator())
		{
			continue;
		}

		++ActivePlayers;

		if (IsPawnInPortal(PlayerState->GetPawn()))
		{
			++PlayersInPortal;
		}
	}

	if (PlayersInPortal == 0)
	{
		EnterDeadline = -1.0;
		return;
	}

	if (PlayersInPortal == ActivePlayers)
	{
		TravelToNextLevel();
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (EnterDeadline < 0.0)
	{
		EnterDeadline = Now + EnterGracePeriod;
	}
	else if (Now >= EnterDeadline)
	{
		// 重开本关：Experience 随之重新加载，全员回到出生点
		StopPortal();
		UModularSystemStatics::PlayNextGame(this);
	}
}

bool AGossipsectGate::IsPawnInPortal(const APawn* Pawn) const
{
	if (!Pawn)
	{
		return false;
	}

	const FVector LocalLocation = PortalVolume->GetComponentTransform().InverseTransformPosition(Pawn->GetActorLocation());
	return FBox(-PortalVolume->GetUnscaledBoxExtent(), PortalVolume->GetUnscaledBoxExtent()).IsInside(LocalLocation);
}

void AGossipsectGate::TravelToNextLevel()
{
	if (ColorTransfer::ServerTravelToLevel(this, NextLevel))
	{
		StopPortal();
	}
}

void AGossipsectGate::StopPortal()
{
	bTravelStarted = true;
	GetWorldTimerManager().ClearTimer(PortalTimer);
}

void AGossipsectGate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGossipsectGate, BlackProgress);
	DOREPLIFETIME(AGossipsectGate, WhiteProgress);
}
