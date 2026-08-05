#include "Kinesis/YanKinesisRegistrySubsystem.h"

#include "Kinesis/YanKinesisTargetComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanKinesisRegistrySubsystem)

UYanKinesisRegistrySubsystem* UYanKinesisRegistrySubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;

	return World ? World->GetSubsystem<UYanKinesisRegistrySubsystem>() : nullptr;
}

void UYanKinesisRegistrySubsystem::RegisterTarget(UYanKinesisTargetComponent* Target)
{
	if (IsValid(Target))
	{
		Targets.AddUnique(Target);
	}
}

void UYanKinesisRegistrySubsystem::UnregisterTarget(UYanKinesisTargetComponent* Target)
{
	Targets.Remove(Target);
}

void UYanKinesisRegistrySubsystem::GatherCandidates(const AActor* InInstigator, const FVector& Origin, float MaxDistance, TArray<UYanKinesisTargetComponent*>& OutTargets)
{
	OutTargets.Reset();

	const float InstigatorRangeSq = MaxDistance * MaxDistance;

	for (int32 Index = Targets.Num() - 1; Index >= 0; --Index)
	{
		UYanKinesisTargetComponent* Target = Targets[Index].Get();

		// 组件随 Actor 销毁而失效：遍历时顺带剔除，省去一份定期整理的逻辑
		if (!IsValid(Target))
		{
			Targets.RemoveAtSwap(Index);
			continue;
		}

		const AActor* TargetActor = Target->GetOwner();
		if (!IsValid(TargetActor))
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(Origin, TargetActor->GetActorLocation());
		if (DistanceSq > InstigatorRangeSq)
		{
			continue;
		}

		// 目标可另行收紧自身被抓取的距离，两侧上限都满足才是候选
		if (Target->MaxControlDistance > 0.f && DistanceSq > FMath::Square(Target->MaxControlDistance))
		{
			continue;
		}

		if (!Target->IsControllableBy(InInstigator))
		{
			continue;
		}

		OutTargets.Add(Target);
	}
}
