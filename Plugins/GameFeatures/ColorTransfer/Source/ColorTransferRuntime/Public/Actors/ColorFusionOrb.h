#pragma once

#include "GameFramework/Actor.h"
#include "ColorTransferTypes.h"

#include "ColorFusionOrb.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class UMaterialInstanceDynamic;
class UNiagaraSystem;
class UStaticMeshComponent;

/**
 * 渡渊的红蓝球：颜色射线注入同色颜料，满 Capacity 后变深色闪烁并进入 ArmedDuration 秒的待引爆窗口，
 * 超时则清空颜料、恢复浅色。两球都处于待引爆时，右键蓄力攻击命中任一球的靶子，
 * 两球相向移动撞到一起，变紫 PurpleDuration 秒后爆炸并前往下一关。
 *
 * 网格由蓝图 SCS 提供，按 OrbMeshName / TargetMeshName 解析；球体材质需带 Color 与 EmissiveStrength 参数。
 * 只在服务器上结算，状态以服务器时间复制，各端自行插值移动与闪烁。
 */
UCLASS(MinimalAPI)
class AColorFusionOrb : public AActor, public IColorShotReceiver
{
	GENERATED_BODY()

public:
	UE_API AColorFusionOrb();

	/**
	 * 右键蓄力攻击命中某个组件时调用，仅在服务器上调用。
	 * @return Component 是否为某个融合球的靶子（无论是否满足引爆条件）
	 */
	static UE_API bool AuthNotifyChargeHit(const UPrimitiveComponent* Component);

	//~Begin IColorShotReceiver Interface
	UE_API virtual void ReceiveColorShot(const FColorShotHit& Shot) override;
	//~End IColorShotReceiver Interface

	//~Begin AActor Interface
	UE_API virtual void PostInitializeComponents() override;
	UE_API virtual void Tick(float DeltaSeconds) override;
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

	UFUNCTION(BlueprintPure, Category = "Yan|ColorTransfer")
	bool IsArmed() const { return bArmed && !IsFusing(); }

	UFUNCTION(BlueprintPure, Category = "Yan|ColorTransfer")
	bool IsFusing() const { return FusionStartTime >= 0.0; }

protected:
	//~Begin AActor Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End AActor Interface

	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer")
	EColorPigment AcceptedPigment = EColorPigment::Red;

	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "1.0"))
	float Capacity = 100.0f;

	/** 注满后等待引爆的时长 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.1", ForceUnits = "s"))
	float ArmedDuration = 6.0f;

	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer")
	FLinearColor IdleColor = FLinearColor(1.0f, 0.45f, 0.45f);

	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer")
	FLinearColor ArmedColor = FLinearColor(0.35f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.1"))
	float BlinkFrequency = 3.0f;

	/** 引爆后两球相向移动到碰撞的时长 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.1", ForceUnits = "s"))
	float FusionMoveDuration = 1.0f;

	/** 碰撞后保持紫色到爆炸的时长 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.0", ForceUnits = "s"))
	float PurpleDuration = 1.0f;

	/** 另一颗球；两边都要互相指向 */
	UPROPERTY(EditInstanceOnly, Category = "Yan|ColorTransfer")
	TObjectPtr<AColorFusionOrb> Partner;

	/** 爆炸后前往的关卡；由被命中的一方发起切关，两颗球都填上以免漏配 */
	UPROPERTY(EditInstanceOnly, Category = "Yan|ColorTransfer")
	TSoftObjectPtr<UWorld> NextLevel;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	TSoftObjectPtr<UNiagaraSystem> ExplosionEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	FName OrbMeshName = TEXT("StaticMesh");

	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	FName TargetMeshName = TEXT("Cube");

private:
	void AuthArm();
	void AuthDisarm();
	void AuthStartFusion(const FVector& MeetLocation);
	void AuthExplode();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExplode(FVector_NetQuantize Location);

	UFUNCTION()
	void OnRep_Visual();

	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	float StoredAmount = 0.0f;

	// 注满后处于待引爆窗口；融合开始后仍保持，以沿用深色
	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	bool bArmed = false;

	// 服务器时间；小于 0 表示未开始融合
	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	double FusionStartTime = -1.0;

	UPROPERTY(Replicated)
	FVector_NetQuantize FusionFrom;

	UPROPERTY(Replicated)
	FVector_NetQuantize FusionTo;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> OrbMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> TargetMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OrbMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> LoadedExplosionEffect;

	FTimerHandle ArmedTimer;
};

#undef UE_API
