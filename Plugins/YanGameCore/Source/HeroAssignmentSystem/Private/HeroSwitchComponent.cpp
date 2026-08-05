#include "HeroSwitchComponent.h"

#include "HeroAssignmentComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HeroSwitchComponent)

UHeroSwitchComponent::UHeroSwitchComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UHeroSwitchComponent::RequestChangeHero(FPrimaryAssetId HeroId)
{
	ServerChangeHero(HeroId);
}

void UHeroSwitchComponent::ServerChangeHero_Implementation(FPrimaryAssetId HeroId)
{
	AController* Controller = GetController<AController>();
	if (!Controller)
	{
		return;
	}

	const UWorld*         World     = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	if (UHeroAssignmentComponent* AssignmentComponent = GameState->FindComponentByClass<UHeroAssignmentComponent>())
	{
		AssignmentComponent->ChangeHeroForController(Controller, HeroId);
	}
}
