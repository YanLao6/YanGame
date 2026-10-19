// 武器刷新台实现。

#include "Weapon/WeaponSpawner.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Equipment/PickupDefinition.h"
#include "GameFramework/Pawn.h"
#include "Inventory/InventoryFragment_SetStats.h"
#include "Inventory/InventoryItemDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "ModularGameplayAbilitiesLogChannels.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WeaponSpawner)

AWeaponSpawner::AWeaponSpawner()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CollisionVolume = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionVolume"));
	CollisionVolume->InitCapsuleSize(80.0f, 80.0f);
	CollisionVolume->OnComponentBeginOverlap.AddDynamic(this, &AWeaponSpawner::OnOverlapBegin);

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(RootComponent);

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(RootComponent);

	WeaponMeshRotationSpeed   = 40.0f;
	CoolDownTime              = 30.0f;
	CheckExistingOverlapDelay = 0.25f;
	bIsWeaponAvailable        = true;
	bReplicates               = true;
}

void AWeaponSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (WeaponDefinition && WeaponDefinition->InventoryItemDefinition)
	{
		CoolDownTime = WeaponDefinition->SpawnCoolDownSeconds;
	}
	else if (const UWorld* World = GetWorld())
	{
		if (!World->IsPlayingReplay())
		{
			UE_LOG(LogModularGameplayAbilities, Error, TEXT("'%s' 未配置有效的武器定义，请在关卡实例上设置。"), *GetNameSafe(this));
		}
	}
}

void AWeaponSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CoolDownTimerHandle);
		World->GetTimerManager().ClearTimer(CheckOverlapsDelayTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AWeaponSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (World->GetTimerManager().IsTimerActive(CoolDownTimerHandle))
	{
		CoolDownPercentage = 1.0f - World->GetTimerManager().GetTimerRemaining(CoolDownTimerHandle) / CoolDownTime;
	}

	WeaponMesh->AddRelativeRotation(FRotator(0.0f, World->GetDeltaSeconds() * WeaponMeshRotationSpeed, 0.0f));
}

void AWeaponSpawner::OnConstruction(const FTransform& Transform)
{
	if (WeaponDefinition != nullptr && WeaponDefinition->DisplayMesh != nullptr)
	{
		WeaponMesh->SetStaticMesh(WeaponDefinition->DisplayMesh);
		WeaponMesh->SetRelativeLocation(WeaponDefinition->WeaponMeshOffset);
		WeaponMesh->SetRelativeScale3D(WeaponDefinition->WeaponMeshScale);
	}
}

void AWeaponSpawner::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	APawn* OverlappingPawn = Cast<APawn>(OtherActor);
	if (GetLocalRole() == ROLE_Authority && bIsWeaponAvailable && OverlappingPawn != nullptr)
	{
		AttemptPickUpWeapon(OverlappingPawn);
	}
}

void AWeaponSpawner::CheckForExistingOverlaps()
{
	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors, APawn::StaticClass());

	for (AActor* OverlappingActor : OverlappingActors)
	{
		AttemptPickUpWeapon(Cast<APawn>(OverlappingActor));
	}
}

void AWeaponSpawner::AttemptPickUpWeapon_Implementation(APawn* Pawn)
{
	if (GetLocalRole() == ROLE_Authority && bIsWeaponAvailable && UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn))
	{
		const TSubclassOf<UInventoryItemDefinition> WeaponItemDefinition = WeaponDefinition ? WeaponDefinition->InventoryItemDefinition : nullptr;
		if (WeaponItemDefinition != nullptr)
		{
			if (GiveWeapon(WeaponItemDefinition, Pawn))
			{
				bIsWeaponAvailable = false;
				SetWeaponPickupVisibility(false);
				PlayPickupEffects();
				StartCoolDown();
			}
		}
	}
}

void AWeaponSpawner::StartCoolDown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(CoolDownTimerHandle, this, &AWeaponSpawner::OnCoolDownTimerComplete, CoolDownTime);
	}
}

void AWeaponSpawner::ResetCoolDown()
{
	UWorld* World = GetWorld();

	if (World)
	{
		World->GetTimerManager().ClearTimer(CoolDownTimerHandle);
	}

	if (GetLocalRole() == ROLE_Authority)
	{
		bIsWeaponAvailable = true;
		PlayRespawnEffects();
		SetWeaponPickupVisibility(true);

		if (World)
		{
			World->GetTimerManager().SetTimer(CheckOverlapsDelayTimerHandle, this, &AWeaponSpawner::CheckForExistingOverlaps, CheckExistingOverlapDelay);
		}
	}

	CoolDownPercentage = 0.0f;
}

void AWeaponSpawner::OnCoolDownTimerComplete()
{
	ResetCoolDown();
}

void AWeaponSpawner::SetWeaponPickupVisibility(bool bShouldBeVisible)
{
	WeaponMesh->SetVisibility(bShouldBeVisible, true);
}

void AWeaponSpawner::PlayPickupEffects_Implementation()
{
	if (WeaponDefinition != nullptr)
	{
		if (USoundBase* PickupSound = WeaponDefinition->PickedUpSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
		}

		if (UNiagaraSystem* PickupEffect = WeaponDefinition->PickedUpEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, PickupEffect, WeaponMesh->GetComponentLocation());
		}
	}
}

void AWeaponSpawner::PlayRespawnEffects_Implementation()
{
	if (WeaponDefinition != nullptr)
	{
		if (USoundBase* RespawnSound = WeaponDefinition->RespawnedSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, RespawnSound, GetActorLocation());
		}

		if (UNiagaraSystem* RespawnEffect = WeaponDefinition->RespawnedEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, RespawnEffect, WeaponMesh->GetComponentLocation());
		}
	}
}

void AWeaponSpawner::OnRep_WeaponAvailability()
{
	if (bIsWeaponAvailable)
	{
		PlayRespawnEffects();
		SetWeaponPickupVisibility(true);
	}
	else
	{
		SetWeaponPickupVisibility(false);
		StartCoolDown();
		PlayPickupEffects();
	}
}

void AWeaponSpawner::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWeaponSpawner, bIsWeaponAvailable);
}

int32 AWeaponSpawner::GetDefaultStatFromItemDef(const TSubclassOf<UInventoryItemDefinition> WeaponItemClass, FGameplayTag StatTag)
{
	if (WeaponItemClass != nullptr)
	{
		if (const UInventoryItemDefinition* WeaponItemCDO = WeaponItemClass->GetDefaultObject<UInventoryItemDefinition>())
		{
			if (const UInventoryFragment_SetStats* ItemStatsFragment = Cast<UInventoryFragment_SetStats>(WeaponItemCDO->FindFragmentByClass(UInventoryFragment_SetStats::StaticClass())))
			{
				return ItemStatsFragment->GetItemStatByTag(StatTag);
			}
		}
	}

	return 0;
}
