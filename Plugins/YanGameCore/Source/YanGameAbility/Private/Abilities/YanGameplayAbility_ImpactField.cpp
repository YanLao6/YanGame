#include "Abilities/YanGameplayAbility_ImpactField.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GeometryCollection/GeometryCollectionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanGameplayAbility_ImpactField)

namespace ImpactFieldConsoleVariables
{
	static float                   DrawImpactDuration = 0.0f;
	static FAutoConsoleVariableRef CVarDrawImpactDuration(
		TEXT("ImpactField.DrawImpactDuration"),
		DrawImpactDuration,
		TEXT("绘制撞击探测与断裂位置的持续时间，单位为秒；大于零时启用。"),
		ECVF_Default);
}

//////////////////////////////////////////////////////////////////////

UYanGameplayAbility_ImpactField::UYanGameplayAbility_ImpactField(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 授予即激活并常驻；断裂只在权威端施加，客户端无需运行本能力
	ActivationPolicy   = EModularAbilityActivationPolicy::OnSpawn;
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UYanGameplayAbility_ImpactField::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// NetExecutionPolicy 已限定服务器执行，此处只是不让例外情况白白采样
	if (!HasAuthority(&ActivationInfo))
	{
		return;
	}
}

void UYanGameplayAbility_ImpactField::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	UnbindAvatarCollisionNotify();
	StopVelocitySampling();

	SpeedSamples.Reset();
	LastTriggerTimeByTarget.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UYanGameplayAbility_ImpactField::OnPawnAvatarSet()
{
	Super::OnPawnAvatarSet();

	// 钩子只向实例分发，此处兜底 CDO 误调：CDO 不该持有任何绑定状态
	if (!IsInstantiated())
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if ((ActorInfo == nullptr) || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	// Avatar 此刻才换成 Pawn，绑定与采样一并重来，旧 Avatar 上的绑定在此过程中解除
	BindAvatarCollisionNotify(ActorInfo);
	StartVelocitySampling();
}

//////////////////////////////////////////////////////////////////////
// 撞击监听

void UYanGameplayAbility_ImpactField::BindAvatarCollisionNotify(const FGameplayAbilityActorInfo* ActorInfo)
{
	UnbindAvatarCollisionNotify();

	const AActor* Avatar = (ActorInfo != nullptr) ? ActorInfo->AvatarActor.Get() : nullptr;
	if (Avatar == nullptr)
	{
		return;
	}

	// Avatar 尚为 PlayerState 时根组件不是碰撞体，此处静默跳过，等 OnPawnAvatarSet 再绑
	UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Avatar->GetRootComponent());
	if (RootPrimitive == nullptr)
	{
		return;
	}

	// Chaos 只为开启了刚体碰撞通知的组件收集撞击事件，故此处强制打开并记下原值
	bRestoreRigidBodyCollisionNotify = RootPrimitive->BodyInstance.bNotifyRigidBodyCollision;
	RootPrimitive->SetNotifyRigidBodyCollision(true);
	RootPrimitive->OnComponentHit.AddDynamic(this, &ThisClass::HandleAvatarHit);

	BoundCollisionComponent = RootPrimitive;
}

void UYanGameplayAbility_ImpactField::UnbindAvatarCollisionNotify()
{
	if (UPrimitiveComponent* RootPrimitive = BoundCollisionComponent.Get())
	{
		RootPrimitive->OnComponentHit.RemoveDynamic(this, &ThisClass::HandleAvatarHit);
		RootPrimitive->SetNotifyRigidBodyCollision(bRestoreRigidBodyCollisionNotify);
	}

	BoundCollisionComponent.Reset();
}

void UYanGameplayAbility_ImpactField::HandleAvatarHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor == nullptr)
	{
		return;
	}

	// 只认 GeometryCollection：断裂应力落在其他碰撞体上没有意义
	if (Cast<UGeometryCollectionComponent>(OtherComp) == nullptr)
	{
		return;
	}

	float ApproachSpeed = 0.0f;
	if (!FindPeakApproachSample(Hit.ImpactNormal, ApproachSpeed) || (ApproachSpeed < MinImpactSpeed))
	{
		return;
	}

	if (!TryMarkTargetTriggered(OtherActor))
	{
		return;
	}

	ApplyImpactStrain(OtherComp, Hit.ImpactPoint, Hit.ImpactNormal, ApproachSpeed);
}

//////////////////////////////////////////////////////////////////////
// 速度采样

void UYanGameplayAbility_ImpactField::StartVelocitySampling()
{
	StopVelocitySampling();

	// 节流表以 SampleElapsedTime 计时，时间基准归零时必须同时清空，否则旧条目会被判为永不过期
	SampleElapsedTime = 0.0f;
	SpeedSamples.Reset();
	LastTriggerTimeByTarget.Reset();

	VelocitySampleTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickSampleVelocity));
}

void UYanGameplayAbility_ImpactField::StopVelocitySampling()
{
	if (VelocitySampleTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(VelocitySampleTickerHandle);
		VelocitySampleTickerHandle.Reset();
	}
}

bool UYanGameplayAbility_ImpactField::TickSampleVelocity(float DeltaTime)
{
	SampleElapsedTime += DeltaTime;

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (Avatar == nullptr)
	{
		return true;
	}

	FImpactSpeedSample& Sample = SpeedSamples.AddDefaulted_GetRef();
	Sample.TimeSeconds         = SampleElapsedTime;
	// 根组件模拟物理时 Pawn 的速度即刚体速度，与撞击同源
	Sample.Velocity            = Avatar->GetVelocity();

	// 窗口外的样本不再参与判定，长时间存活也不会让数组无限增长
	const float CutoffTime = SampleElapsedTime - SpeedSampleWindow;
	SpeedSamples.RemoveAll([CutoffTime](const FImpactSpeedSample& Other)
	{
		return Other.TimeSeconds < CutoffTime;
	});

	return true;
}

bool UYanGameplayAbility_ImpactField::FindPeakApproachSample(const FVector& ImpactNormal, float& OutApproachSpeed) const
{
	const FVector NormalDir = ImpactNormal.GetSafeNormal();
	if (NormalDir.IsNearlyZero())
	{
		return false;
	}

	bool bFound      = false;
	OutApproachSpeed = 0.0f;

	for (const FImpactSpeedSample& Sample : SpeedSamples)
	{
		// 法线朝向取决于事件由哪一方注册，取绝对值只关心垂直于撞击面的速度分量
		const float Speed = FMath::Abs(FVector::DotProduct(Sample.Velocity, NormalDir));
		if (!bFound || (Speed > OutApproachSpeed))
		{
			bFound           = true;
			OutApproachSpeed = Speed;
		}
	}

	return bFound;
}

bool UYanGameplayAbility_ImpactField::TryMarkTargetTriggered(AActor* Target)
{
	// 过期与失效的条目一并清除，剩下的即仍在间隔内的目标，节流表不随场景累积
	for (auto It = LastTriggerTimeByTarget.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || ((SampleElapsedTime - It.Value()) >= RetriggerDelay))
		{
			It.RemoveCurrent();
		}
	}

	if (LastTriggerTimeByTarget.Contains(Target))
	{
		return false;
	}

	LastTriggerTimeByTarget.Add(Target, SampleElapsedTime);
	return true;
}

//////////////////////////////////////////////////////////////////////
// 断裂

void UYanGameplayAbility_ImpactField::ApplyImpactStrain(UPrimitiveComponent* TargetComponent, const FVector& ImpactPoint, const FVector& ImpactNormal, float ApproachSpeed) const
{
	UWorld* const World = GetWorld();
	if ((World == nullptr) || (TargetComponent == nullptr))
	{
		return;
	}

	const FVector NormalDir = ImpactNormal.GetSafeNormal();
	if (NormalDir.IsNearlyZero())
	{
		return;
	}

	// 法线由被撞方指向自身，起止点沿其跨过撞击面，sweep 必然扫到目标簇
	const FVector SweepStart = ImpactPoint + (NormalDir * ProbeDistance);
	const FVector SweepEnd   = ImpactPoint - (NormalDir * ProbeDistance);

	// 按目标自身的对象类型查询，不依赖 GeometryCollection 对 Visibility 通道的配置
	const FCollisionObjectQueryParams ObjectParams(TargetComponent->GetCollisionObjectType());

	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(YanImpactStrainProbe), false, GetAvatarActorFromActorInfo());

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(Hits, SweepStart, SweepEnd, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(ProbeRadius), QueryParams);

	const float SpeedRange  = MaxImpactSpeed - MinImpactSpeed;
	const float StrainAlpha = (SpeedRange > UE_SMALL_NUMBER) ? FMath::Clamp((ApproachSpeed - MinImpactSpeed) / SpeedRange, 0.0f, 1.0f) : 1.0f;

	// 碎块沿射入方向溅开
	const FVector BreakingDirection = -NormalDir;

	for (const FHitResult& ProbeHit : Hits)
	{
		UGeometryCollectionComponent* const Collection = Cast<UGeometryCollectionComponent>(ProbeHit.GetComponent());

		// 只有 trace/sweep 路径的命中才带得出 ItemIndex，取不到就无从指定要断的簇
		if ((Collection == nullptr) || (ProbeHit.Item == INDEX_NONE))
		{
			continue;
		}

		// 应力中心取刚体碰撞的接触点而非 sweep 命中点：sweep 只负责取出 ItemIndex，
		// 其起点若已与目标重叠，命中点会退化成 sweep 起点，使断裂位置沿法线偏出 ProbeDistance
		ApplyStrainToCluster(*Collection, ProbeHit.Item, ImpactPoint, BreakingDirection, StrainAlpha);
	}

#if ENABLE_DRAW_DEBUG
	if (ImpactFieldConsoleVariables::DrawImpactDuration > 0.0f)
	{
		const float Duration = ImpactFieldConsoleVariables::DrawImpactDuration;

		// 青球为应力实际作用范围，绿点为其中心（刚体碰撞接触点）
		DrawDebugSphere(World, ImpactPoint, StrainRadius, 16, FColor::Cyan, false, Duration, 0, 1.0f);
		DrawDebugPoint(World, ImpactPoint, 16.0f, FColor::Green, false, Duration);
		DrawDebugDirectionalArrow(World, SweepStart, SweepEnd, 20.0f, FColor::Yellow, false, Duration, 0, 2.0f);

		// 红点为 sweep 命中点，与绿点分离即说明 sweep 起点已陷入目标
		for (const FHitResult& ProbeHit : Hits)
		{
			DrawDebugPoint(World, ProbeHit.ImpactPoint, 12.0f, FColor::Red, false, Duration);
		}
	}
#endif
}

void UYanGameplayAbility_ImpactField::ApplyStrainToCluster(UGeometryCollectionComponent& Collection, int32 ItemIndex, const FVector& Location, const FVector& BreakingDirection, float StrainAlpha) const
{
	// 应力只落在半径内的子块上，不沿连接图向邻块扩散
	constexpr int32 PropagationDepth  = 0;
	constexpr float PropagationFactor = 0.0f;

	if (bInternalStrainOnly)
	{
		Collection.ApplyInternalStrain(ItemIndex, Location, StrainRadius, PropagationDepth, PropagationFactor, InternalStrainRelief);
		return;
	}

	const float Strain = FMath::Lerp(MinStrain, MaxStrain, StrainAlpha);
	Collection.ApplyExternalStrain(ItemIndex, Location, StrainRadius, PropagationDepth, PropagationFactor, Strain);

	// 初速度只加给本次应力已越过内聚强度的子块，故必须紧跟在施加应力之后
	Collection.ApplyBreakingLinearVelocity(ItemIndex, BreakingDirection * BreakingVelocityScale * StrainAlpha);
}
