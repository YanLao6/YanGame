#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YanHookProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHookHitSignature, AYanHookProjectile*, Projectile, const FHitResult&, Hit);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHookMissedSignature, AYanHookProjectile*, Projectile);

/**
 * 钩锁头投射物。
 *
 * 服务器 Spawn 并负责权威命中检测（bReplicates=true，命中/销毁由服务器裁决并复制）。
 * 为避免每帧复制 Transform 的网络开销，采用"复制初速度 + 客户端本地模拟"方案：
 *   服务器调用 Launch() 后，ReplicatedInitialVelocity 通过 RepNotify 同步到客户端，
 *   客户端收到后在本地启动 ProjectileMovementComponent 进行弹道模拟，实现流畅视觉效果。
 * 超出 MaxTravelDistance 仍未命中时，服务器广播 OnHookMissed 并销毁；Destroy 复制到客户端。
 */
UCLASS()
class YANGAMEMOVER_API AYanHookProjectile : public AActor
{
	GENERATED_BODY()

public:
	AYanHookProjectile();

	/** 沿 Direction 方向以 Speed（cm/s）发射钩锁头，调用前须先完成 SpawnActor */
	UFUNCTION(BlueprintCallable, Category = "Grappling")
	void Launch(FVector Direction, float Speed);

	/** 命中世界几何体时广播（命中结果含世界坐标 ImpactPoint） */
	UPROPERTY(BlueprintAssignable, Category = "Grappling")
	FOnHookHitSignature OnHookHit;

	/** 超出最大射程未命中时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Grappling")
	FOnHookMissedSignature OnHookMissed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** 最大飞行距离（cm），超出后触发 OnHookMissed 并自毁 */
	UPROPERTY(EditDefaultsOnly, Category = "Grappling", meta = (ClampMin = "100"))
	float MaxTravelDistance = 3500.f;

	/** 碰撞球半径（cm） */
	UPROPERTY(EditDefaultsOnly, Category = "Grappling", meta = (ClampMin = "1"))
	float CollisionRadius = 8.f;

	/**
	 * 发射音效：服务器 Launch() 后初速度经 RepNotify 复制到各客户端的那一刻播放，
	 * 实现"服务器成功发射并复制到本地时播音效"。监听端 host 由 Launch() 内补播。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Grappling")
	TObjectPtr<USoundBase> LaunchSound;

protected:
	//~Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Destroyed() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

private:
	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// 收到初速度复制后，在客户端启动本地弹道模拟
	UFUNCTION()
	void OnRep_InitialVelocity();

	// 复制初速度：服务器 Launch() 后写入，客户端经 RepNotify 驱动本地 ProjectileMovement
	UPROPERTY(ReplicatedUsing = OnRep_InitialVelocity)
	FVector ReplicatedInitialVelocity = FVector::ZeroVector;

	// 发射起点，用于服务器累积已飞行距离
	FVector LaunchLocation = FVector::ZeroVector;
	bool    bLaunched      = false;
};
