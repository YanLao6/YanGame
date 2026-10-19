#include "Actor/YanKineticSummon.h"

#include "YanMoverAngelscriptLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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

	// 自发光的空腔不该遮挡光照：投影会在球体上留下一圈环形暗影
	WakeComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WakeComp"));
	WakeComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WakeComp->SetCastShadow(false);
	WakeComp->SetupAttachment(CollisionComp);
	UpdateWakeTransform(WakeReferenceSpeed);

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

	UpdateWakeTransform(ProjectileMovement->MaxSpeed);

	SetLifeSpan(LifeSpanAfterLaunch);

	OnLaunched();
}

void AYanKineticSummon::OnLaunched()
{}

void AYanKineticSummon::UpdateWakeTransform(float Speed)
{
	if (!WakeComp)
	{
		return;
	}

	// 基础网格的半径：拖尾由单位球拉伸而成，换网格后仍按其包围球换算
	const UStaticMesh* WakeMesh   = WakeComp->GetStaticMesh();
	const float        MeshRadius = WakeMesh ? WakeMesh->GetBounds().SphereRadius : 50.f;
	if (MeshRadius <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 低速时不缩成一点，高速时不拉到失真
	const float LengthScale = FMath::Clamp(Speed / FMath::Max(WakeReferenceSpeed, 1.f), 0.35f, 1.8f);
	const float HalfLength  = FMath::Max(WakeLength * LengthScale * 0.5f, UE_KINDA_SMALL_NUMBER);
	const float Lateral     = CollisionRadius * WakeRadiusRatio;

	WakeComp->SetRelativeScale3D(FVector(HalfLength / MeshRadius, Lateral / MeshRadius, Lateral / MeshRadius));

	// 前端埋进球体，接缝才不会露在轮廓上；余下部分向 -X 拖出
	WakeComp->SetRelativeLocation(FVector(-(HalfLength - CollisionRadius * 0.7f), 0.f, 0.f));
}

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

	UpdateWakeTransform(ProjectileMovement->MaxSpeed);
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

	UYanMoverAngelscriptLibrary::ApplyLaunchEffectToTarget(InstigatorASC, TargetASC, ImpactLaunchEffect, ImpactVelocity);
}
