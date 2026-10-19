#include "Player/YanMinimapComponent.h"

#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "ModularTeamAgentInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanMinimapComponent)

UYanMinimapComponent::UYanMinimapComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

bool UYanMinimapComponent::GetSelfMarker(FYanMinimapMarker& OutMarker) const
{
	const AController* OwningController = GetController<AController>();
	if (OwningController == nullptr)
	{
		return false;
	}

	APawn* OwningPawn = OwningController->GetPawn();
	if (OwningPawn == nullptr)
	{
		return false;
	}

	OutMarker = MakeMarker(OwningPawn);
	return true;
}

void UYanMinimapComponent::GetOtherMarkers(TArray<FYanMinimapMarker>& OutMarkers) const
{
	OutMarkers.Reset();

	const UWorld* World = GetWorld();
	const AController* OwningController = GetController<AController>();
	if (World == nullptr || OwningController == nullptr)
	{
		return;
	}

	const APawn* OwningPawn = OwningController->GetPawn();

	for (FConstControllerIterator It = World->GetControllerIterator(); It; ++It)
	{
		AController* Other = It->Get();
		if (Other == nullptr || Other == OwningController)
		{
			continue;
		}

		// 只标记已生成的 Pawn：处在重生间隙的对手在场上没有位置可言
		APawn* OtherPawn = Other->GetPawn();
		if (OtherPawn == nullptr || OtherPawn == OwningPawn)
		{
			continue;
		}

		OutMarkers.Add(MakeMarker(OtherPawn));
	}
}

float UYanMinimapComponent::GetViewYawDegrees() const
{
	if (const AController* OwningController = GetController<AController>())
	{
		return OwningController->GetControlRotation().Yaw;
	}

	return 0.f;
}

FYanMinimapMarker UYanMinimapComponent::MakeMarker(AActor* Target) const
{
	FYanMinimapMarker Marker;
	if (Target == nullptr)
	{
		return Marker;
	}

	Marker.Actor       = Target;
	Marker.YawDegrees  = Target->GetActorRotation().Yaw;

	const FVector  Location = Target->GetActorLocation();
	const FVector2D Offset  = (FVector2D(Location.X, Location.Y) - MapCenter) / MapExtent;

	// 越界目标夹到边界而非丢弃：被打飞出场的对手正是最需要在小地图上看见的时刻
	Marker.NormalizedPosition = FVector2D(FMath::Clamp(Offset.X, -1.f, 1.f), FMath::Clamp(Offset.Y, -1.f, 1.f));
	Marker.bClampedToEdge     = !Offset.Equals(Marker.NormalizedPosition);

	if (const APawn* TargetPawn = Cast<APawn>(Target))
	{
		if (const IModularTeamAgentInterface* TeamAgent = Cast<IModularTeamAgentInterface>(TargetPawn->GetPlayerState()))
		{
			Marker.TeamId = GenericTeamIdToInteger(TeamAgent->GetGenericTeamId());
		}
	}

	return Marker;
}
