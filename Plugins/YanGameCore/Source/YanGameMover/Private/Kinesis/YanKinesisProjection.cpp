#include "Kinesis/YanKinesisProjection.h"

#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

namespace YanKinesis
{
	bool GetPlayerViewportSize(const APlayerController* OwningController, FVector2D& OutSize)
	{
		const ULocalPlayer* LocalPlayer = OwningController ? OwningController->GetLocalPlayer() : nullptr;
		if (!LocalPlayer || !LocalPlayer->ViewportClient)
		{
			return false;
		}

		FVector2D FullViewportSize = FVector2D::ZeroVector;
		LocalPlayer->ViewportClient->GetViewportSize(FullViewportSize);

		OutSize = FullViewportSize * LocalPlayer->Size;

		return OutSize.X > 0.f && OutSize.Y > 0.f;
	}

	bool ProjectAnchorToScreen(const APlayerController* OwningController, const FVector& ViewLocation, const FVector& ViewForward, const FVector& AnchorLocation, FVector2D& OutScreenPosition)
	{
		if (!OwningController || FVector::DotProduct(ViewForward, AnchorLocation - ViewLocation) <= 0.f)
		{
			return false;
		}

		return OwningController->ProjectWorldLocationToScreen(AnchorLocation, OutScreenPosition, /*bPlayerViewportRelative=*/true);
	}
}
