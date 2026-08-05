#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YanKineticSummon.generated.h"

#define UE_API YANGAMEMOVER_API

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UGameplayEffect;

/**
 * 念力召唤物：赫的行为，也是苍的基类。
 *
 * 由蓄力发射技能在玩家松手的瞬间于视点前方生成并即刻射出，没有待命阶段。
 * 召唤物在蓄力期间尚不存在，因而不会随玩家移动被留在起蓄的位置。
 *
 * 网络模型沿用投射物的既有做法：服务器 Spawn 与裁决，初速度经 RepNotify 复制，
 * 客户端据此本地模拟弹道，避免每帧同步 Transform。
 * 初速度在 Spawn 当帧写入，随首次复制一并抵达。
 *
 * 赫命中敌方时给予其一份与自身飞行方向一致的冲量：方向即撞上去的方向，
 * 大小在射出瞬间由蓄力量定死，不随飞行距离或命中时机变化。
 */
UCLASS(MinimalAPI)
class AYanKineticSummon : public AActor
{
	GENERATED_BODY()

public:
	UE_API AYanKineticSummon();

	/**
	 * 服务器调用：以给定世界速度射出，须在 Spawn 当帧调用，初速度方能随首次复制抵达客户端。
	 *
	 * @param WorldVelocity     世界空间初速度
	 * @param InEffectMagnitude 本次发射的作用强度（赫为命中冲量 cm/s，苍为牵引强度）；
	 *                          非正表示发射方未指定，沿用类上配置的默认值
	 */
	UFUNCTION(BlueprintCallable, Category = "Kinesis")
	UE_API void Launch(FVector WorldVelocity, float InEffectMagnitude = 0.f);

	/** 存活时长上限（秒），射出后超时自毁，避免未命中的召唤物长期滞留 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", meta = (ClampMin = "0"))
	float LifeSpanAfterLaunch = 6.f;

	/** 碰撞球半径（cm） */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", meta = (ClampMin = "1"))
	float CollisionRadius = 24.f;

	/**
	 * 命中敌方时施加的击飞 GE，须使用 UMoverLaunchExecutionCalculation。
	 * 留空即不施加命中冲量——苍靠牵引而非撞击生效，正是这么配的。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis|Impact")
	TSubclassOf<UGameplayEffect> ImpactLaunchEffect;

	/** 命中时给予敌方的速度大小（cm/s），Launch 传入正的作用强度时以其为准 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis|Impact", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float ImpactSpeed = 900.f;

	/**
	 * 命中后禁止敌方落地的时长（秒），0 表示不禁止。
	 * 冲量若在地面模式下结算会被地面摩擦当帧吃掉，故命中后先把目标压在空中一小段时间。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis|Impact", meta = (ClampMin = "0", ForceUnits = "s"))
	float ImpactNoLandingDuration = 0.3f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

protected:
	//~Begin AActor Interface
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

	/** 射出瞬间在服务器触发，派生类据此启动自己的持续行为 */
	UE_API virtual void OnLaunched();

	/** 本次发射的作用强度：非正时回落到类上配置的默认值 */
	UE_API float ResolveEffectMagnitude(float DefaultMagnitude) const;

	UFUNCTION(BlueprintImplementableEvent)
	UE_API void K2_OnHit();
	
	// 由发射方按蓄力量给出，仅服务器使用，故无须复制
	float EffectMagnitude = 0.f;

private:
	UFUNCTION()
	UE_API void HandleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// 收到初速度复制后在客户端启动本地弹道模拟
	UFUNCTION()
	UE_API void OnRep_LaunchVelocity();

	// 命中方向由飞行速度给出：召唤物停下后不再有方向可言，故命中当帧即取
	UE_API void ApplyImpactToTarget(AActor* HitActor) const;

	// 复制初速度：服务器 Launch() 后写入，客户端经 RepNotify 驱动本地 ProjectileMovement。
	// 非零即表示已射出，Launch 据此拒绝重复调用
	UPROPERTY(ReplicatedUsing = OnRep_LaunchVelocity)
	FVector ReplicatedLaunchVelocity = FVector::ZeroVector;
};

#undef UE_API
