#include "Actors/ColorExecutionZone.h"

#include "ColorTransferQueries.h"
#include "Components/BoxComponent.h"
#include "Components/ColorPigmentRulesComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorExecutionZone)

AColorExecutionZone::AColorExecutionZone()
{
	ExecutionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ExecutionVolume"));
	ExecutionVolume->InitBoxExtent(FVector(100.0f));
	ExecutionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ExecutionVolume->SetCollisionObjectType(ECC_WorldDynamic);
	ExecutionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	// 判定走定时轮询查询，不依赖引擎的 Overlap 委托
	ExecutionVolume->SetGenerateOverlapEvents(false);
	RootComponent = ExecutionVolume;
}

void AColorExecutionZone::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(UpdateTimer, this, &ThisClass::AuthUpdatePawnsInVolume, UpdateInterval, /*bLoop=*/true);
	}
}

void AColorExecutionZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(UpdateTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void AColorExecutionZone::AuthUpdatePawnsInVolume()
{
	for (APawn* Pawn : ColorTransfer::GatherPawnsInVolume(ExecutionVolume))
	{
		const APlayerState* PlayerState = Pawn->GetPlayerState();
		if (UColorPigmentRulesComponent* Rules = PlayerState ? PlayerState->FindComponentByClass<UColorPigmentRulesComponent>() : nullptr)
		{
			Rules->AuthRequestDeath();
		}
	}
}
