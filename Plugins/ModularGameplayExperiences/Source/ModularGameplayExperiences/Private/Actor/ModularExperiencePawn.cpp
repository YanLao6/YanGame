// Copyright Chronicler.


#include "Actor/ModularExperiencePawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularExperiencePawn)

AModularExperiencePawn::AModularExperiencePawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 默认关闭逐帧 Tick，降低空转开销
	//@todo 在 Blueprint 层对误开 Tick 给出开发期提示
	PrimaryActorTick.bCanEverTick          = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	PawnExtensionComponent = CreateDefaultSubobject<UModularPawnComponent>(TEXT("ModularPawnComponent"));
}

void AModularExperiencePawn::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{}

bool AModularExperiencePawn::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	return false;
}

bool AModularExperiencePawn::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	return false;
}

bool AModularExperiencePawn::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	return false;
}

void AModularExperiencePawn::BeginPlay()
{
	Super::BeginPlay();
}

void AModularExperiencePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AModularExperiencePawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	PawnExtensionComponent->HandleControllerChanged();
}

void AModularExperiencePawn::UnPossessed()
{
	PawnExtensionComponent->HandleControllerChanged();

	Super::UnPossessed();
}


void AModularExperiencePawn::OnRep_Controller()
{
	Super::OnRep_Controller();

	PawnExtensionComponent->HandleControllerChanged();
}

void AModularExperiencePawn::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	PawnExtensionComponent->HandlePlayerStateReplicated();
}

void AModularExperiencePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PawnExtensionComponent->SetupPlayerInputComponent();
}
