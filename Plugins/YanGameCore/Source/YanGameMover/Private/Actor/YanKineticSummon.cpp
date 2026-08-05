#include "Actor/YanKineticSummon.h"

#include "YanMoverAngelscriptLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "MoverComponent.h"
#include "Messages/VerbMessageHelpers.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanKineticSummon)

AYanKineticSummon::AYanKineticSummon()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	// 不逐帧推 Transform，改由 ReplicatedLaunchVelocity 驱动客户端本地模拟
	AActor::SetReplicateMovement(false);

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->InitSphereRadius(CollisionRadius);
	CollisionComp->SetCollisionProfileName(TEXT("Pawn"));
	CollisionComp->OnComponentHit.AddDynamic(this, &AYanKineticSummon::HandleHit);
	SetRootComponent(CollisionComp);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetupAttachment(CollisionComp);

	// 悬停待命：初速度为零，Launch 前不移动
	ProjectileMovement                           = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->ProjectileGravityScale   = 0.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->InitialSpeed             = 0.f;
	ProjectileMovement->MaxSpeed                 = 0.f;
	ProjectileMovement->bShouldBounce            = false;
}

void AYanKineticSummon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYanKineticSummon, ReplicatedLaunchVelocity);
}

void AYanKineticSummon::Launch(FVector WorldVelocity, float InEffectMagnitude)
{
	if (!ReplicatedLaunchVelocity.IsNearlyZero() || WorldVelocity.IsNearlyZero())
	{
		return;
	}

	EffectMagnitude = InEffectMagnitude;

	// 双向 ignore：召唤物不撞召唤者，召唤者也不撞它
	if (AActor* OwnerActor = GetOwner())
	{
		CollisionComp->IgnoreActorWhenMoving(OwnerActor, true);
		if (UPrimitiveComponent* OwnerRoot = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent()))
		{
			OwnerRoot->IgnoreActorWhenMoving(this, true);
		}
	}

	ReplicatedLaunchVelocity = WorldVelocity; // Spawn 当帧写入，随首次复制抵达客户端并触发本地模拟

	ProjectileMovement->Velocity = WorldVelocity;
	ProjectileMovement->MaxSpeed = WorldVelocity.Size();
	ProjectileMovement->UpdateComponentVelocity();

	SetLifeSpan(LifeSpanAfterLaunch);

	OnLaunched();
}

void AYanKineticSummon::OnLaunched()
{}

float AYanKineticSummon::ResolveEffectMagnitude(float DefaultMagnitude) const
{
	return EffectMagnitude > 0.f ? EffectMagnitude : DefaultMagnitude;
}

void AYanKineticSummon::OnRep_LaunchVelocity()
{
	if (ReplicatedLaunchVelocity.IsNearlyZero())
	{
		return;
	}

	ProjectileMovement->Velocity = ReplicatedLaunchVelocity;
	ProjectileMovement->MaxSpeed = ReplicatedLaunchVelocity.Size();
	ProjectileMovement->UpdateComponentVelocity();
}

void AYanKineticSummon::HandleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	K2_OnHit();
	
	// 命中裁决与效果施加一律在服务器；客户端的本地模拟只负责表现
	if (!HasAuthority() || !OtherActor || OtherActor == GetOwner())
	{
		return;
	}

	ApplyImpactToTarget(OtherActor);
}

void AYanKineticSummon::ApplyImpactToTarget(AActor* HitActor) const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !ImpactLaunchEffect)
	{
		return;
	}

	UAbilitySystemComponent* InstigatorASC = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(OwnerActor);
	UAbilitySystemComponent* TargetASC     = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(HitActor);

	if (!InstigatorASC || !TargetASC)
	{
		return;
	}

	// 撞上去的方向即冲量方向；大小在射出瞬间已定，与飞了多远、何时命中无关
	const FVector TravelDirection = ProjectileMovement->Velocity.GetSafeNormal();
	if (TravelDirection.IsNearlyZero())
	{
		return;
	}

	const FVector ImpactVelocity = TravelDirection * ResolveEffectMagnitude(ImpactSpeed);

	// 先压住落地：目标若停在地面模式，冲量当帧就会被地面摩擦抵消
	if (ImpactNoLandingDuration > 0.f)
	{
		if (UMoverComponent* TargetMover = HitActor->FindComponentByClass<UMoverComponent>())
		{
			UYanMoverAngelscriptLibrary::ApplyNoLandingToTarget(TargetMover, ImpactNoLandingDuration);
		}
	}

	// 瞬时速度无持续语义，DurationMs 传 0
	UYanMoverAngelscriptLibrary::ApplyLaunchEffectToTarget(InstigatorASC, TargetASC, ImpactLaunchEffect, ImpactVelocity, /*DurationMs=*/0.f);
}
