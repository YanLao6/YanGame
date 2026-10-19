#include "Actors/ColorTransferCheckpoint.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "ActorComponent/ModularAbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameplayAbilities/ModularAbilitySet.h"
#include "GameMode/ColorTransferSpawningComponent.h"
#include "ModularTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorTransferCheckpoint)

AColorTransferCheckpoint::AColorTransferCheckpoint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ActivationVolume"));
	ActivationVolume->SetupAttachment(GetRootComponent());
	ActivationVolume->SetBoxExtent(FVector(120.f, 120.f, 160.f));
	ActivationVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ActivationVolume->SetCollisionObjectType(ECC_WorldDynamic);
	ActivationVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	ActivationVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ActivationVolume->SetGenerateOverlapEvents(true);
}

void AColorTransferCheckpoint::BeginPlay()
{
	Super::BeginPlay();
	ActivationVolume->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleActivationVolumeBeginOverlap);

	ActivationVolume->UpdateOverlaps();
}

void AColorTransferCheckpoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ActivationVolume->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandleActivationVolumeBeginOverlap);
	for (auto& Entry : TutorialWidgets)
	{
		GetWorldTimerManager().ClearTimer(Entry.Value.RemovalTimer);
		Entry.Value.ExtensionHandle.Unregister();
	}
	TutorialWidgets.Empty();
	Super::EndPlay(EndPlayReason);
}

void AColorTransferCheckpoint::ShowTutorialWidget(APlayerController* PlayerController)
{
	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	UUIExtensionSubsystem* ExtensionSubsystem = GetWorld()->GetSubsystem<UUIExtensionSubsystem>();
	if (!LocalPlayer || !ExtensionSubsystem || !TutorialWidgetClass)
	{
		return;
	}

	const FGameplayTag SlotTag = FGameplayTag::RequestGameplayTag(TEXT("HUD.Slot.Tutorial"));
	FTutorialWidgetState& State = TutorialWidgets.FindOrAdd(LocalPlayer);
	if (!State.ExtensionHandle.IsValid())
	{
		State.ExtensionHandle = ExtensionSubsystem->RegisterExtensionAsWidgetForContext(SlotTag, LocalPlayer, TutorialWidgetClass, -1);
	}

	// 重复进入刷新显示时长，每个本地玩家独立计时。
	GetWorldTimerManager().SetTimer(State.RemovalTimer,
		FTimerDelegate::CreateUObject(this, &ThisClass::RemoveTutorialWidget, TWeakObjectPtr<ULocalPlayer>(LocalPlayer)), 10.f, false);
}

void AColorTransferCheckpoint::RemoveTutorialWidget(TWeakObjectPtr<ULocalPlayer> LocalPlayer)
{
	if (FTutorialWidgetState* State = TutorialWidgets.Find(LocalPlayer))
	{
		GetWorldTimerManager().ClearTimer(State->RemovalTimer);
		State->ExtensionHandle.Unregister();
		TutorialWidgets.Remove(LocalPlayer);
	}
}

void AColorTransferCheckpoint::HandleActivationVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(OtherActor);
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return;
	}

	ShowTutorialWidget(Cast<APlayerController>(Controller));
	if (!HasAuthority())
	{
		return;
	}

	if (AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		if (UColorTransferSpawningComponent* Spawning = GameState->FindComponentByClass<UColorTransferSpawningComponent>())
		{
			Spawning->SetPlayerCheckpoint(Controller, this);
		}
	}

	UModularAbilitySystemComponent* ASC = Cast<UModularAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn));
	if (!ASC)
	{
		ASC = Cast<UModularAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Controller->PlayerState));
	}
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	const UModularTeamSubsystem* TeamSubsystem = GetWorld()->GetSubsystem<UModularTeamSubsystem>();
	if (!TeamSubsystem)
	{
		return;
	}

	const int32 TeamId = TeamSubsystem->FindTeamFromObject(Controller);
	const TObjectPtr<UModularAbilitySet>* ConfiguredSet = TeamId != INDEX_NONE ? TeamAbilitySets.Find(TeamId) : nullptr;
	UModularAbilitySet* NewAbilitySet = ConfiguredSet ? ConfiguredSet->Get() : nullptr;
	if (FColorTransferCheckpointAbilityGrant* ExistingGrant = AbilityGrants.Find(ASC))
	{
		if (ExistingGrant->AbilitySet == NewAbilitySet)
		{
			return;
		}
		ExistingGrant->GrantedHandles.TakeFromAbilitySystem(ASC);
		AbilityGrants.Remove(ASC);
	}

	if (NewAbilitySet)
	{
		FColorTransferCheckpointAbilityGrant& Grant = AbilityGrants.Add(ASC);
		Grant.AbilitySet = NewAbilitySet;
		NewAbilitySet->GiveToAbilitySystem(ASC, &Grant.GrantedHandles, this);
	}
}
