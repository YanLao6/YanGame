#pragma once

#include "GameFramework/Actor.h"

#include "ColorPurpleProjectile.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class UNiagaraSystem;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * 茈：紫爆倒计时中右键蓄力、松手射出的直线弹体，命中其他玩家即令其死亡，碰到任何阻挡或飞满射程后消失。
 *
 * 生成后静止且无碰撞，由技能挂到施法者手上，AuthLaunch 后才开始飞行。
 * 检测走颜色射线的 Weapon_Capsule 通道，与左键射线看到的阻挡一致；命中只在服务器上结算。
 */
UCLASS(MinimalAPI)
class AColorPurpleProjectile : public AActor
{
	GENERATED_BODY()

public:
	UE_API AColorPurpleProjectile();

	/** 仅在服务器上调用：脱离插槽，沿 Direction 飞出 */
	UE_API void AuthLaunch(const FVector& Direction);

	//~Begin AActor Interface
	UE_API virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;
	//~End AActor Interface

protected:
	//~Begin AActor Interface
	UE_API virtual void BeginPlay() override;
	//~End AActor Interface

	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UProjectileMovementComponent> Movement;

	/** 射程（cm） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer", Meta = (ClampMin = "1.0"))
	float MaxRange = 5000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	TSoftObjectPtr<UNiagaraSystem> ImpactEffect;

private:
	UFUNCTION()
	void OnProjectileStop(const FHitResult& ImpactResult);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastImpact(FVector_NetQuantize Location);

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> LoadedImpactEffect;
};

#undef UE_API
