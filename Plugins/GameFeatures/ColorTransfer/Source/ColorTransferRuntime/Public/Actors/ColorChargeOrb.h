#pragma once

#include "GameFramework/Actor.h"

#include "ColorChargeOrb.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class USoundBase;
class UStaticMeshComponent;

/**
 * 右键蓄力时手上的球。
 *
 * 由服务器生成并挂到施法者骨骼网格的插槽上，蓄力档位与进度复制到各端，
 * 各端据此缩放球体、在档位提升时播放音效。松手后可由 AuthFlyTo 脱手飞向终点：
 * 未绑定 OnArrived 时到达即自毁，绑定后交由调用方处置（如附着到命中者身上）。
 */
UCLASS(MinimalAPI)
class AColorChargeOrb : public AActor
{
	GENERATED_BODY()

public:
	UE_API AColorChargeOrb();

	/** 仅在服务器上调用。Alpha 为蓄力进度 0..1，Tier 为当前档位。 */
	UE_API void AuthSetCharge(float Alpha, int32 Tier);

	/**
	 * 仅在服务器上调用：脱离插槽，以 FlySpeed 飞向目标。
	 * TargetComponent 有效时追踪其 TargetSocket 的实时位置，否则飞向固定点 TargetLocation。
	 */
	UE_API void AuthFlyTo(const FVector& TargetLocation, USceneComponent* TargetComponent = nullptr, FName TargetSocket = NAME_None);

	/** 飞行到达时触发，仅服务器。 */
	FSimpleDelegate OnArrived;

	float GetChargeAlpha() const { return ChargeAlpha; }

	//~Begin AActor Interface
	UE_API virtual void Tick(float DeltaSeconds) override;
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

protected:
	//~Begin AActor Interface
	UE_API virtual void BeginPlay() override;
	//~End AActor Interface

	/** 纯表现，无碰撞 */
	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UStaticMeshComponent> OrbMesh;

	/** 蓄力进度 0 与 1 时的缩放 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	FVector2D ScaleRange = FVector2D(0.1f, 0.1f);

	/** 按档位下标播放的音效，档位提升时播放，数量不足时沿用最后一个 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	TArray<TObjectPtr<USoundBase>> TierSounds;

	/** 脱手后的飞行速度（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer", Meta = (ClampMin = "1.0"))
	float FlySpeed = 4000.0f;

private:
	UFUNCTION()
	void OnRep_ChargeAlpha();

	UFUNCTION()
	void OnRep_ChargeTier();

	UPROPERTY(ReplicatedUsing = OnRep_ChargeAlpha)
	float ChargeAlpha = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_ChargeTier)
	uint8 ChargeTier = 0;

	FVector GetFlyTarget() const;

	// 仅服务器：飞行目标，未脱手时不使用
	FVector FlyTargetLocation = FVector::ZeroVector;

	TWeakObjectPtr<USceneComponent> FlyTargetComponent;

	FName FlyTargetSocket;

	bool bFlying = false;
};

#undef UE_API
