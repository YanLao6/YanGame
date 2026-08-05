#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "YanKinesisRegistrySubsystem.generated.h"

#define UE_API YANGAMEMOVER_API

class UYanKinesisTargetComponent;

/**
 * 念力可控物登记表。
 *
 * 世界中每个挂载 UYanKinesisTargetComponent 的 Actor 在 BeginPlay 时登记、EndPlay 时注销，
 * 念力的目标选取与屏幕指示器扫描都查这张表，不再逐次做物理重叠查询。
 *
 * 这样换来两点：扫描频率可以提高到指示器需要的程度而不付出查询代价；
 * 可控物不必拥有碰撞体，纯逻辑物件同样能被念力抓取。
 *
 * 登记项以弱引用持有：组件被销毁而未及注销时（如关卡流送卸载）自动失效，
 * 既不阻止 GC，也不会留下悬垂指针。
 */
UCLASS(MinimalAPI)
class UYanKinesisRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 从任意世界上下文取得本子系统，世界无效时返回 nullptr */
	static UE_API UYanKinesisRegistrySubsystem* Get(const UObject* WorldContextObject);

	UE_API void RegisterTarget(UYanKinesisTargetComponent* Target);
	UE_API void UnregisterTarget(UYanKinesisTargetComponent* Target);

	/**
	 * 收集 Origin 周围当前允许被 InInstigator 控制的目标。
	 *
	 * 距离上限取调用方与目标各自配置中较严的一个：技能给出施法者的能力半径，
	 * 目标给出自身被抓取的距离限制，两者都成立才是候选。
	 * 已失效的登记项在此顺带清理，无须额外的定期整理。
	 *
	 * @param InInstigator 施法者，自身与其所属组件一律排除
	 * @param Origin       距离判定的原点，通常为施法者视点
	 * @param MaxDistance  施法者一侧的距离上限（cm）
	 */
	UE_API void GatherCandidates(const AActor* InInstigator, const FVector& Origin, float MaxDistance, TArray<UYanKinesisTargetComponent*>& OutTargets);

private:
	TArray<TWeakObjectPtr<UYanKinesisTargetComponent>> Targets;
};

#undef UE_API
