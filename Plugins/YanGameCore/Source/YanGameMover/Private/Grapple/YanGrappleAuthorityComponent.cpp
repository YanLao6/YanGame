#include "Grapple/YanGrappleAuthorityComponent.h"

#include "ChaosMover/ChaosMoverLog.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "MoverComponent.h"
#include "YanMoverAngelscriptLibrary.h"
#include "YanMoverDataModelTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanGrappleAuthorityComponent)

UYanGrappleAuthorityComponent::UYanGrappleAuthorityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UYanGrappleAuthorityComponent::BeginPlay()
{
	Super::BeginPlay();

	// 裁决是服务器独有的职责，客户端上连 tick 都不必开
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		SetComponentTickEnabled(false);
		return;
	}

	CachedMoverComp = Owner->FindComponentByClass<UMoverComponent>();
	if (!CachedMoverComp)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UYanGrappleAuthorityComponent 所在 Actor 未挂载 MoverComponent，钩锁牵引不会生效"));
		SetComponentTickEnabled(false);
	}
}

void UYanGrappleAuthorityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CachedMoverComp)
	{
		return;
	}

	const FYanCharacterInputs* YanInputs = CachedMoverComp->GetLastInputCmd().InputCollection.FindDataByType<FYanCharacterInputs>();
	if (!YanInputs || !YanInputs->bGrapplePullsTarget)
	{
		// 意图落回则本轮结束：已下发的牵引到期自行失效，无须回收。
		// 计时器留在到期状态，下一次钩中时首帧即可下发，不必先等满一个间隔
		ResolvedTargetMoverComp.Reset();
		TimeSinceLastPull = RefreshIntervalSeconds;
		return;
	}

	// 认定失败时同样按间隔重试，否则「附近找不到目标」会退化成逐帧的碰撞查询
	TimeSinceLastPull += DeltaTime;
	if (TimeSinceLastPull < RefreshIntervalSeconds)
	{
		return;
	}

	TimeSinceLastPull = 0.0f;
	ResolveAndPull(*YanInputs);
}

void UYanGrappleAuthorityComponent::ResolveAndPull(const FYanCharacterInputs& YanInputs)
{
	// 目标只在本轮的第一帧认定一次：逐次认定会在两人交错而过时把牵引转移到旁人身上
	if (!ResolvedTargetMoverComp.IsValid())
	{
		APawn* Target = FindPawnNear(YanInputs.GrappleTargetLocation);
		if (!Target)
		{
			UE_LOG(LogChaosMover, Verbose, TEXT("%s 的钩锁未认定到目标：上传位置附近无带 Mover 的 Pawn"), *GetNameSafe(GetOwner()));
			return;
		}

		ResolvedTargetMoverComp = Target->FindComponentByClass<UMoverComponent>();
		if (!ResolvedTargetMoverComp.IsValid())
		{
			return;
		}
	}

	// 汇合点取发起端上传的值：它依赖被钩者的位置，而该位置在发起端是插值外推的结果，
	// 服务器另算一份必然与发起者自己正在收敛的那个点不一致
	UYanMoverAngelscriptLibrary::ApplyGrapplePullToTarget(ResolvedTargetMoverComp.Get(),
	                                                      YanInputs.GrappleAnchorPoint,
	                                                      PullAcceleration,
	                                                      MaxPullSpeed,
	                                                      StopDistance,
	                                                      /*DurationMs=*/RefreshIntervalSeconds * 2000.0f);
}

APawn* UYanGrappleAuthorityComponent::FindPawnNear(const FVector& Location) const
{
	AActor* OwnerActor = GetOwner();
	UWorld* World      = OwnerActor ? OwnerActor->GetWorld() : nullptr;
	if (!World || VerifyRadius <= 0.0f)
	{
		return nullptr;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GrappleTargetResolve), /*bTraceComplex=*/false);
	QueryParams.AddIgnoredActor(OwnerActor);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps,
	                                Location,
	                                FQuat::Identity,
	                                FCollisionObjectQueryParams(ECC_Pawn),
	                                FCollisionShape::MakeSphere(VerifyRadius),
	                                QueryParams);

	APawn* BestTarget      = nullptr;
	float  BestDistSquared = TNumericLimits<float>::Max();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		APawn* Candidate = Cast<APawn>(Overlap.GetActor());

		// 同一 Actor 可能有多个组件落在球内，取最近者即可，无须去重
		if (!Candidate || Candidate == OwnerActor)
		{
			continue;
		}

		// 牵引经 Mover 落地，没有 MoverComponent 的 Pawn 拉不动，不该被认定为目标
		if (!Candidate->FindComponentByClass<UMoverComponent>())
		{
			continue;
		}

		const float DistSquared = FVector::DistSquared(Candidate->GetActorLocation(), Location);
		if (DistSquared < BestDistSquared)
		{
			BestDistSquared = DistSquared;
			BestTarget      = Candidate;
		}
	}

	return BestTarget;
}
