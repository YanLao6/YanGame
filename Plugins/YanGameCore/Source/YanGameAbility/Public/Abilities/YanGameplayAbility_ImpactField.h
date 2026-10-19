#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GameplayAbilities/ModularGameplayAbility.h"

#include "YanGameplayAbility_ImpactField.generated.h"

#define UE_API YANGAMEABILITY_API

class UGeometryCollectionComponent;
class UPrimitiveComponent;

/**
 * 被动能力：以足够快的速度撞上 GeometryCollection 时，直接对撞击处的簇施加断裂应力与飞散初速度。
 *
 * 授予即激活并常驻（ActivationPolicy 须为 OnSpawn），激活期间监听 Avatar 根碰撞体的刚体撞击。
 * 角色由 ChaosMover 驱动，移动解算在物理线程完成，Mover 自身的 OnHandleImpact 不会被调用，
 * 因此撞击只能取自 Chaos 的刚体碰撞事件。
 *
 * ASC 挂在 PlayerState 上时，授予时刻的 Avatar 仍是 PlayerState，拿不到碰撞体，
 * 故真正的绑定放在 OnPawnAvatarSet；此时也不能因绑定失败而结束能力——
 * 能力一旦结束，Pawn 就绪后的那次 OnSpawn 重试便不再有实例可回调。
 * 该钩子在 Avatar 更换（重生、切换英雄）时会再次到来，绑定因此始终跟随当前 Pawn。
 *
 * 物理异步步进使碰撞事件回到游戏线程时速度已被撞击削去，故判定用的是撞击前一小段窗口内的采样速度。
 * 断裂只在权威端施加，客户端的碎裂由 GeometryCollection 自身的复制驱动。
 *
 * 施加断裂需要 GeometryCollection 的 ItemIndex，而刚体碰撞通知不提供它：
 * 通知路径的 FHitResult::Item 取自 FRigidBodyCollisionInfo::BodyIndex，
 * GeometryCollection 没有 FBodyInstance，该字段恒为 INDEX_NONE。
 * 只有 trace/sweep 路径才会从 GeometryCollection 的物理代理反查出真实 ItemIndex，
 * 故撞击成立后需就地补一次 sweep，其唯一目的是取得 ItemIndex。
 */
UCLASS(MinimalAPI)
class UYanGameplayAbility_ImpactField : public UModularGameplayAbility
{
	GENERATED_BODY()

public:
	UE_API UYanGameplayAbility_ImpactField(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UGameplayAbility Interface
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	UE_API virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End UGameplayAbility Interface

	//~Begin UModularGameplayAbility Interface
	UE_API virtual void OnPawnAvatarSet() override;
	//~End UModularGameplayAbility Interface

protected:
	/** 触发所需的撞击速度，只计速度在撞击面法线上的分量，因此贴着表面擦过不会触发。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact", meta = (ForceUnits = "cm/s", ClampMin = "0.0"))
	float MinImpactSpeed = 1200.0f;

	/** 应力达到上限所需的撞击速度；超过此速度不再增强，避免高速下碎裂强度失控。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact", meta = (ForceUnits = "cm/s", ClampMin = "0.0"))
	float MaxImpactSpeed = 4000.0f;

	/** 同一目标两次触发之间的最小间隔；持续接触会逐帧产生撞击事件，靠它收敛为单次。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact", meta = (ForceUnits = "s", ClampMin = "0.0"))
	float RetriggerDelay = 0.5f;

	/** 撞击速度的回看窗口；物理异步步进下事件滞后于碰撞，只取当帧速度会显著偏小。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact", meta = (ForceUnits = "s", ClampMin = "0.01"))
	float SpeedSampleWindow = 0.15f;

	/** 取 ItemIndex 用的 sweep 球半径；需覆盖撞击点附近的表面起伏，过小会扫空。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact|Probe", meta = (ForceUnits = "cm", ClampMin = "1.0"))
	float ProbeRadius = 40.0f;

	/** sweep 沿撞击面法线向两侧各延伸的距离；只需跨过撞击面，不做弹道预测。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact|Probe", meta = (ForceUnits = "cm", ClampMin = "1.0"))
	float ProbeDistance = 60.0f;

	/** 应力作用半径；簇内子块按与撞击点的距离入选，半径越大一次撞碎的范围越广。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact|Strain", meta = (ForceUnits = "cm", ClampMin = "0.0"))
	float StrainRadius = 300.0f;

	/** 撞击速度处于下限时施加的外部应力，低于子块内聚强度则不会断裂。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact|Strain", meta = (ClampMin = "0.0"))
	float MinStrain = 200000.0f;

	/** 撞击速度达到上限时施加的外部应力。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact|Strain", meta = (ClampMin = "0.0"))
	float MaxStrain = 16500000.0f;

	/** 改为削弱子块内聚强度而非施加外部应力；此路径不给碎块初速度，表现为撞松而非撞飞。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact|Strain")
	bool bInternalStrainOnly = false;

	/** bInternalStrainOnly 下每次撞击削减的内聚强度，与撞击速度无关。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact|Strain", meta = (EditCondition = "bInternalStrainOnly", ClampMin = "0.0"))
	float InternalStrainRelief = 50000.0f;

	/** 断裂瞬间沿撞击方向叠加给碎块的初速度，按应力强度缩放；竖直分量取小值，碎块才向前溅开而非掀起。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact|Strain", meta = (ForceUnits = "cm/s"))
	FVector BreakingVelocityScale = FVector(88.0f, 88.0f, 22.0f);

private:
	// 一帧的 Avatar 速度采样
	struct FImpactSpeedSample
	{
		float   TimeSeconds = 0.0f;
		FVector Velocity    = FVector::ZeroVector;
	};

	UFUNCTION()
	UE_API void HandleAvatarHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// 绑定与解绑 Avatar 根碰撞体的撞击通知，成对调用；解绑时还原通知开关
	void BindAvatarCollisionNotify(const FGameplayAbilityActorInfo* ActorInfo);
	void UnbindAvatarCollisionNotify();

	// 每帧记录 Avatar 速度；返回 true 保持 ticker 存活
	bool TickSampleVelocity(float DeltaTime);

	void StartVelocitySampling();
	void StopVelocitySampling();

	// 在窗口内找出撞击面法线方向上分量最大的一次采样，无采样时返回 false
	bool FindPeakApproachSample(const FVector& ImpactNormal, float& OutApproachSpeed) const;

	// 目标是否已过再次触发的间隔；返回 true 时同时记下本次触发时间
	bool TryMarkTargetTriggered(AActor* Target);

	// 跨撞击面 sweep 出带 ItemIndex 的命中，逐个施加应力
	void ApplyImpactStrain(UPrimitiveComponent* TargetComponent, const FVector& ImpactPoint, const FVector& ImpactNormal, float ApproachSpeed) const;

	// 对单个簇施加应力；外部应力路径同帧补一次飞散初速度
	void ApplyStrainToCluster(UGeometryCollectionComponent& Collection, int32 ItemIndex, const FVector& Location, const FVector& BreakingDirection, float StrainAlpha) const;

private:
	// 速度采样窗口，只保留 SpeedSampleWindow 之内的记录
	TArray<FImpactSpeedSample> SpeedSamples;

	// 采样累计时长，同时作为节流表的时间基准，不依赖 World 时间
	float SampleElapsedTime = 0.0f;

	FTSTicker::FDelegateHandle VelocitySampleTickerHandle;

	// 仍处于再次触发间隔内的目标及其触发时刻
	TMap<TWeakObjectPtr<AActor>, float> LastTriggerTimeByTarget;

	TWeakObjectPtr<UPrimitiveComponent> BoundCollisionComponent;

	// 绑定前的刚体碰撞通知开关，解绑时还原，避免影响其他依赖该开关的系统
	bool bRestoreRigidBodyCollisionNotify = false;
};

#undef UE_API
