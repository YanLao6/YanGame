#include "Actors/ColorPurpleProjectile.h"

#include "ColorTransferTypes.h"
#include "Components/ColorPigmentRulesComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorPurpleProjectile)

AColorPurpleProjectile::AColorPurpleProjectile()
{
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(20.0f);
	// 移动扫掠以自身 ObjectType 作为检测通道，设成射线通道即与左键射线命中同样的目标；
	// 手持期间关闭碰撞，否则会挡住别人的左键射线
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collision->SetCollisionObjectType(ColorTransfer::ShotTraceChannel);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->InitialSpeed = 3000.0f;
	Movement->MaxSpeed = 3000.0f;
	Movement->ProjectileGravityScale = 0.0f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bAutoActivate = false;

	bReplicates = true;
	SetReplicatingMovement(true);
}

void AColorPurpleProjectile::AuthLaunch(const FVector& Direction)
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorRotation(Direction.Rotation());

	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Movement->Velocity = Direction * Movement->InitialSpeed;
	Movement->Activate();

	SetLifeSpan(MaxRange / FMath::Max(Movement->InitialSpeed, 1.0f));
}

void AColorPurpleProjectile::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	Super::PostNetReceiveVelocity(NewVelocity);

	// 移动组件不复制，客户端按复制下来的速度自行模拟，飞行才平滑；碰撞仍关着，命中只由服务器结算
	Movement->Velocity = NewVelocity;
	if (!NewVelocity.IsNearlyZero() && !Movement->IsActive())
	{
		Movement->Activate();
	}
}

void AColorPurpleProjectile::BeginPlay()
{
	Super::BeginPlay();

	Collision->IgnoreActorWhenMoving(GetInstigator(), true);

	if (GetNetMode() != NM_DedicatedServer)
	{
		LoadedImpactEffect = ImpactEffect.LoadSynchronous();
	}

	if (HasAuthority())
	{
		Movement->OnProjectileStop.AddDynamic(this, &ThisClass::OnProjectileStop);
	}
}

void AColorPurpleProjectile::OnProjectileStop(const FHitResult& ImpactResult)
{
	const APawn* Victim = Cast<APawn>(ImpactResult.GetActor());
	const APlayerState* VictimState = Victim ? Victim->GetPlayerState() : nullptr;
	if (UColorPigmentRulesComponent* Rules = VictimState ? VictimState->FindComponentByClass<UColorPigmentRulesComponent>() : nullptr)
	{
		Rules->AuthRequestDeath();
	}

	MulticastImpact(ImpactResult.ImpactPoint);
	Destroy();
}

void AColorPurpleProjectile::MulticastImpact_Implementation(FVector_NetQuantize Location)
{
	if (GetNetMode() != NM_DedicatedServer && LoadedImpactEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, LoadedImpactEffect, Location);
	}
}
