#include "Actor/YanHookProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanHookProjectile)

AYanHookProjectile::AYanHookProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;

	// 不依赖 bReplicateMovement 逐帧推 Transform，改由 ReplicatedInitialVelocity 驱动客户端本地模拟
	AActor::SetReplicateMovement(false);

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->InitSphereRadius(CollisionRadius);
	CollisionComp->SetCollisionProfileName(TEXT("BlockAll"));
	CollisionComp->OnComponentHit.AddDynamic(this, &AYanHookProjectile::HandleHit);
	SetRootComponent(CollisionComp);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetupAttachment(CollisionComp);

	ProjectileMovement                           = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->ProjectileGravityScale   = 0.f; // 无重力，匀速直线飞行
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->InitialSpeed             = 0.f;
	ProjectileMovement->MaxSpeed                 = 0.f;
	ProjectileMovement->bShouldBounce            = false;
}

void AYanHookProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AYanHookProjectile, ReplicatedInitialVelocity);
}

void AYanHookProjectile::BeginPlay()
{
	Super::BeginPlay();
}

void AYanHookProjectile::Launch(FVector Direction, float Speed)
{
	LaunchLocation = GetActorLocation() + GetActorForwardVector() * 100.f;
	bLaunched      = true;

	// 双向 ignore：投射物不撞 Owner，Owner 也不撞投射物
	if (AActor* OwnerActor = GetOwner())
	{
		CollisionComp->IgnoreActorWhenMoving(OwnerActor, true);
		if (UPrimitiveComponent* OwnerRoot = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent()))
		{
			OwnerRoot->IgnoreActorWhenMoving(this, true);
		}
	}

	const FVector Velocity    = Direction.GetSafeNormal() * Speed;
	ReplicatedInitialVelocity = Velocity; // RepNotify 触发客户端本地模拟

	ProjectileMovement->SetVelocityInLocalSpace(FVector::ZeroVector);
	ProjectileMovement->Velocity = Velocity;
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->UpdateComponentVelocity();

	// 监听端 host 不经 OnRep（RepNotify 仅在远端触发），在此补播发射音效；专用服务器无本地玩家则跳过
	if (LaunchSound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LaunchSound, GetActorLocation());
	}
}

void AYanHookProjectile::OnRep_InitialVelocity()
{
	if (ReplicatedInitialVelocity.IsNearlyZero())
	{
		return;
	}

	// 客户端同步 Owner ignore，防止投射物与发射者产生视觉碰撞
	if (AActor* OwnerActor = GetOwner())
	{
		CollisionComp->IgnoreActorWhenMoving(OwnerActor, true);
		if (UPrimitiveComponent* OwnerRoot = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent()))
		{
			OwnerRoot->IgnoreActorWhenMoving(this, true);
		}
	}

	LaunchLocation = GetActorLocation();
	bLaunched      = true;

	ProjectileMovement->Velocity = ReplicatedInitialVelocity;
	ProjectileMovement->MaxSpeed = ReplicatedInitialVelocity.Size();
	ProjectileMovement->UpdateComponentVelocity();

	// 服务器发射成功、初速度复制到本端的这一刻播放发射音效（各客户端各播一次）
	if (LaunchSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LaunchSound, GetActorLocation());
	}
}

void AYanHookProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bLaunched)
	{
		return;
	}

	// 超出最大射程后的裁决（Broadcast 和 Destroy）仅在服务器执行；
	// 客户端 Projectile 由服务器复制的 Destroy 触发销毁
	if (!HasAuthority())
	{
		return;
	}

	const float TraveledDistance = FVector::Dist(GetActorLocation(), LaunchLocation);
	if (TraveledDistance >= MaxTravelDistance)
	{
		OnHookMissed.Broadcast(this);
		Destroy();
	}
}

void AYanHookProjectile::Destroyed()
{
	// 解除双向 ignore，避免 Owner Pawn 在投射物销毁后仍持有悬空的 ignore 引用
	if (AActor* OwnerActor = GetOwner())
	{
		CollisionComp->IgnoreActorWhenMoving(OwnerActor, false);
		if (UPrimitiveComponent* OwnerRoot = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent()))
		{
			OwnerRoot->IgnoreActorWhenMoving(this, false);
		}
	}
	Super::Destroyed();
}

void AYanHookProjectile::HandleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 停止运动，广播命中，由 Ability 决定后续生命周期
	ProjectileMovement->StopMovementImmediately();
	OnHookHit.Broadcast(this, Hit);
}
