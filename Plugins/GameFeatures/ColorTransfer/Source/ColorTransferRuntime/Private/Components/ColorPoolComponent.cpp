#include "Components/ColorPoolComponent.h"

#include "Attributes/ColorPigmentSet.h"
#include "Effects/ColorPigmentEffects.h"
#include "ColorTransferQueries.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorPoolComponent)

UColorPoolComponent::UColorPoolComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision, /*bUpdatePhysicsFilterData=*/false);
	SetGenerateOverlapEvents(false);
	InitBoxExtent(FVector(100.0f, 100.0f, 150.0f));
}

void UColorPoolComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner()->HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(InjectTimer, this, &ThisClass::InjectPawnsInVolume, 1.0f, /*bLoop=*/true);
	}
}

void UColorPoolComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(InjectTimer);

	Super::EndPlay(EndPlayReason);
}

void UColorPoolComponent::InjectPawnsInVolume()
{
	for (APawn* Pawn : ColorTransfer::GatherPawnsInVolume(this))
	{
		if (UColorPigmentSet* PigmentSet = UColorPigmentSet::FindForActor(Pawn))
		{
			ColorTransfer::AuthApplyInjection(*PigmentSet, Pigment, AmountPerSecond, GetOwner());
		}
	}
}
