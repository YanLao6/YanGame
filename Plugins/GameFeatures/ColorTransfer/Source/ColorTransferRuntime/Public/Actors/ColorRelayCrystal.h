#pragma once

#include "Engine/StaticMeshActor.h"
#include "ColorTransferTypes.h"

#include "ColorRelayCrystal.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class AColorWindZone;
class UBoxComponent;
class UMaterialInstanceDynamic;
class UNiagaraSystem;

/**
 * 中继晶体：颜色射线射入的颜料按栈存放，站进 AbsorbVolume 的角色按先进后出每秒吸走栈顶颜料。
 *
 * 存量达到 RequiredAmounts 即完成：永久关闭 WindsToDeactivate，可选变紫后自爆。
 * 完成后不再接收与放出颜料。只在服务器上结算，颜料栈复制给各端驱动 HintMesh 的颜色。
 */
UCLASS(MinimalAPI)
class AColorRelayCrystal : public AStaticMeshActor, public IColorShotReceiver
{
	GENERATED_BODY()

public:
	UE_API explicit AColorRelayCrystal(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin IColorShotReceiver Interface
	UE_API virtual void ReceiveColorShot(const FColorShotHit& Shot) override;
	//~End IColorShotReceiver Interface

	//~Begin AActor Interface
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

	UFUNCTION(BlueprintPure, Category = "Yan|ColorTransfer")
	UE_API float GetStoredAmount(EColorPigment Pigment) const;

	UFUNCTION(BlueprintPure, Category = "Yan|ColorTransfer")
	UE_API float GetTotalStoredAmount() const;

protected:
	//~Begin AActor Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End AActor Interface

	/** 提示晶体颜色，材质需带 Color 与 EmissiveStrength 参数 */
	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UStaticMeshComponent> HintMesh;

	/** 吸收范围，只用于查询 */
	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UBoxComponent> AbsorbVolume;

	/** 接受的颜料；为空表示接受任意颜料。只接受一种时 HintMesh 固定显示该颜色 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer")
	TArray<EColorPigment> AcceptedPigments;

	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "1.0"))
	float Capacity = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.0"))
	float AbsorbPerSecond = 10.0f;

	/** 完成条件：每种颜料的最低存量；为空则永不完成 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer")
	TMap<EColorPigment, float> RequiredAmounts;

	/** 完成时永久关闭的风场 */
	UPROPERTY(EditInstanceOnly, Category = "Yan|ColorTransfer")
	TArray<TObjectPtr<AColorWindZone>> WindsToDeactivate;

	/** 完成后变紫并在 ExplodeDelay 秒后自爆销毁 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer")
	bool bExplodeOnComplete = false;

	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.0", ForceUnits = "s", EditCondition = "bExplodeOnComplete"))
	float ExplodeDelay = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	TSoftObjectPtr<UNiagaraSystem> ExplosionEffect;

private:
	// 服务器定时让区域内的角色吸收栈顶颜料
	void AuthAbsorbTick();

	void AuthCheckCompletion();
	void AuthExplode();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExplode(FVector_NetQuantize Location);

	UFUNCTION()
	void OnRep_Visual();

	bool AcceptsPigment(EColorPigment Pigment) const;

	// 晶体内的颜料栈，末尾为栈顶
	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	TArray<FColorPigmentLayer> StoredLayers;

	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	bool bCompleted = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HintMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> LoadedExplosionEffect;

	FTimerHandle AbsorbTimer;
	FTimerHandle ExplodeTimer;
};

#undef UE_API
