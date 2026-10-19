#pragma once

#include "GameFramework/Actor.h"
#include "ColorTransferTypes.h"
#include "Engine/NetSerialization.h"

#include "ColorShotBeam.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNiagaraSystem;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * 颜色射线的表现：一条从射手视点出发、长度不超过 MaxRange 的直线。
 *
 * 由服务器在射击技能激活期间生成，技能结束即销毁。只复制颜色与瞄准方向，
 * 射线由各端每帧按 PredictPath 自行计算——本地射手从视点指向准星焦点（修正第三人称视差）并上报服务器，
 * 其余端用服务器保存的方向。命中判定与表现共用 PredictPath，看到的射线即判定的射线。
 * 命中表现（溅水特效与音效）同样由各端按本地路径自行播放，节奏与服务器结算间隔一致。
 */
UCLASS(MinimalAPI, NotBlueprintable)
class AColorShotBeam : public AActor
{
	GENERATED_BODY()

public:
	UE_API AColorShotBeam();

	/**
	 * 计算直线路径：沿 Direction 最远 MaxRange，遇到 Weapon_Capsule 阻挡即止。
	 * OutPoints 恒为起点与终点（命中点或极限距离处）两个点。
	 * @return 是否命中
	 */
	static UE_API bool PredictPath(const UWorld* World, AActor* Shooter, const FVector& Start, const FVector& Direction, float MaxRange, TArray<FVector>& OutPoints, FHitResult& OutHit);

	/** 仅在服务器上调用，更新射出的颜色。 */
	UE_API void AuthSetPigment(EColorPigment InPigment);

	/** 当前瞄准方向；服务器上为射手最近一次上报的结果，上报到达前为生成时的初始方向。 */
	FVector GetAimDirection() const { return AimDirection; }

	/** 仅在生成时设置，射手的首次上报到达前以此结算。 */
	void SetAimDirection(const FVector& InAimDirection) { AimDirection = InAimDirection; }

	/** 仅在生成时设置。 */
	void SetMaxRange(float InMaxRange) { MaxRange = InMaxRange; }

	/** 仅在生成时设置。 */
	void SetImpactInterval(float InImpactInterval) { ImpactInterval = InImpactInterval; }

	//~Begin AActor Interface
	UE_API virtual void Tick(float DeltaSeconds) override;
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

protected:
	//~Begin AActor Interface
	UE_API virtual void BeginPlay() override;
	//~End AActor Interface

	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	TSoftObjectPtr<UStaticMesh> SegmentMesh;

	/** 需带 Color 与 EmissiveStrength 参数 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	TSoftObjectPtr<UMaterialInterface> SegmentMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.1"))
	float Thickness = 4.0f;

	/** 需带 User.Color 与 User.SpawnCount 参数，粒子沿本地 Z 轴（命中法线）喷溅 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer|Impact")
	TSoftObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer|Impact")
	TSoftObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer|Impact", Meta = (ClampMin = "1"))
	int32 ImpactSpawnCount = 16;

	/** 命中普通表面时的表现强度，同时缩放音量与水珠数量；命中角色或机关时恒为 1 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer|Impact", Meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SurfaceImpactScale = 0.4f;

private:
	UFUNCTION()
	void OnRep_Pigment();

	UFUNCTION(Server, Unreliable)
	void ServerSetAimDirection(FVector_NetQuantizeNormal InAimDirection);

	// 本地射手的瞄准方向：从视点指向相机中心射线命中的焦点；取不到相机时退回 ControlRotation
	FVector ComputeLocalAimDirection(const APawn* Shooter) const;

	// 本地射手的方向变化明显时才上报，避免每帧发 RPC
	void UpdateLocalAim(const APawn* Shooter);

	// 按路径点数量补齐或隐藏线段
	void UpdateSegments(const TArray<FVector>& Points);

	// 距上次播放满 ImpactInterval 才会再次播放
	void TryPlayImpact(const FHitResult& Hit);

	UPROPERTY(ReplicatedUsing = OnRep_Pigment)
	EColorPigment Pigment = EColorPigment::None;

	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal AimDirection = FVector::ForwardVector;

	UPROPERTY(Replicated)
	float MaxRange = 2000.0f;

	UPROPERTY(Replicated)
	float ImpactInterval = 0.2f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Segments;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BeamMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> LoadedImpactEffect;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LoadedImpactSound;

	double LastImpactTime = -UE_BIG_NUMBER;

	// 本地射手最近一次上报给服务器的方向
	FVector LastSentAimDirection = FVector::ZeroVector;
};

#undef UE_API
