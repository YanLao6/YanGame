#pragma once

#include "GameFramework/Actor.h"

#include "MoverAttractionActor.generated.h"

#define UE_API YANGAMEMOVER_API

class UMoverComponent;

/**
 * 服务器驱动的径向牵引来源，排除 Instigator。
 * 子类提供作用半径并显式开启牵引；捕获与连续位移由目标 ChaosMover 的 SyncState 和移动模式负责。
 * 可独立启用视角操控：方向复用 Instigator 的 Mover 输入，客户端只提供推拉意图。
 */
UCLASS(Abstract, MinimalAPI)
class AMoverAttractionActor : public AActor
{
	GENERATED_BODY()

public:
	UE_API AMoverAttractionActor();

	/** 仅服务器：开始按 AttractionRefreshInterval 刷新牵引，重复调用不会重置计时。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Yan|Mover|Attraction")
	UE_API void StartAttraction();

	/** 仅服务器：停止刷新并释放所有接触过的目标，可重复调用；EndPlay 自动调用。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Yan|Mover|Attraction")
	UE_API void StopAttraction();

	/** 仅服务器：脱离挂接并开启视角操控；Owner 应为 Instigator，以便拥有端提交输入。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Yan|Mover|Attraction|Control")
	UE_API void StartControl();

	/** 仅服务器：停止来源自身的视角操控，牵引场继续生效。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Yan|Mover|Attraction|Control")
	UE_API void StopControl();

	/** 拥有端提交推拉意图：正数推远、负数拉近、零保持距离，仅在状态变化时发送。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Attraction|Control")
	UE_API void SubmitControlInput(int32 InThrottle);

	/** 是否处于视角操控阶段。 */
	UFUNCTION(BlueprintPure, Category = "Yan|Mover|Attraction|Control")
	bool IsControlled() const { return bControlled; }

	/** 当前作用半径（cm）；默认零，由子类按业务提供。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Yan|Mover|Attraction")
	UE_API float GetAttractRadius() const;

	//~Begin AActor Interface
	UE_API virtual void Tick(float DeltaSeconds) override;
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

protected:
	//~Begin AActor Interface
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End AActor Interface

	UE_API virtual float GetAttractRadius_Implementation() const;

	// 仅在拥有端的操控阶段调用；子类将本地输入映射为 SubmitControlInput。
	UE_API virtual void SendLocalControl();

	// 当前范围内的 Pawn，按 Actor 去重并排除 Instigator；不代表模拟线程中的捕获状态。
	UE_API TSet<APawn*> GatherAttractedPawns() const;

	// 只在推拉状态改变时发送；可靠传输确保松开后的零输入不会因丢包丢失。
	UFUNCTION(Server, Reliable)
	UE_API void ServerUpdateControl(int8 InThrottle);

	/** 此半径内锁定来源，离开外圈后仍持续飞行跟随。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Mover|Attraction", Meta = (ClampMin = "0.0"))
	float InnerRadius = 50.0f;

	/** 外圈拉近与内圈跟随的期望速度上限（cm/s），同时受目标飞行速度上限约束。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Mover|Attraction", Meta = (ClampMin = "0.0"))
	float PullSpeed = 600.0f;

	/** 中心快照刷新间隔；连续位移由目标 ChaosMover 的物理模拟步解算。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Mover|Attraction", Meta = (ClampMin = "0.01", ForceUnits = "s"))
	float AttractionRefreshInterval = 0.05f;

	/** 来源追向视角目标点的速度上限（cm/s），由服务器推进。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Mover|Attraction|Control", Meta = (ClampMin = "0.0"))
	float MoveSpeed = 100.0f;

	/** 推拉意图改变视点距离的速度（cm/s）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Mover|Attraction|Control", Meta = (ClampMin = "0.0"))
	float PushSpeed = 100.0f;

	/** 目标点距服务器 Pawn 视点的范围（cm）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Mover|Attraction|Control")
	FVector2D AimDistanceRange = FVector2D(100.0f, 5000.0f);

private:
	UFUNCTION()
	void OnRep_Controlled();

	void MoveTowardsAim(float DeltaSeconds);
	void RefreshAttraction();
	void ReleaseAttraction(UMoverComponent* Mover);

	UPROPERTY(ReplicatedUsing = OnRep_Controlled)
	bool bControlled = false;

	// 服务器保存当前意图；客户端仅缓存已发送值以去重，2 表示尚未发送。
	int8  Throttle         = 0;
	int8  LastSentThrottle = 2;
	float AimDistance      = -1.0f;

	// 已接触目标持续接收中心快照；捕获状态仅由目标 Mover 的 SyncState 持有。
	TSet<TWeakObjectPtr<UMoverComponent>> AttractedMovers;

	FVector      LastAttractionLocation   = FVector::ZeroVector;
	double       LastAttractionSampleTime = -1.0;
	uint32       AttractionUpdateSequence = 0;
	FTimerHandle AttractionTimer;
};

#undef UE_API
