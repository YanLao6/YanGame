#include "ColorTransferQueries.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

TSet<APawn*> ColorTransfer::GatherPawnsInShape(const UWorld* World, const FVector& Location, const FQuat& Rotation, const FCollisionShape& Shape)
{
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Location, Rotation, FCollisionObjectQueryParams(ECC_Pawn), Shape);

	TSet<APawn*> Pawns;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (APawn* Pawn = Cast<APawn>(Overlap.GetActor()))
		{
			Pawns.Add(Pawn);
		}
	}

	return Pawns;
}

TSet<APawn*> ColorTransfer::GatherPawnsInVolume(const UPrimitiveComponent* Volume)
{
	return GatherPawnsInShape(Volume->GetWorld(), Volume->GetComponentLocation(), Volume->GetComponentQuat(), Volume->GetCollisionShape());
}

bool ColorTransfer::GetLocalCameraAim(const APawn* Pawn, bool bStartAtPawn, FVector& OutStart, FVector& OutDirection)
{
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return false;
	}

	FRotator CameraRotation;
	PC->GetPlayerViewPoint(OutStart, CameraRotation);
	OutDirection = CameraRotation.Vector();

	if (bStartAtPawn)
	{
		OutStart += ((Pawn->GetPawnViewLocation() - OutStart) | OutDirection) * OutDirection;
	}

	return true;
}

double ColorTransfer::GetServerTime(const UWorld* World)
{
	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}
