#pragma once

#include "GameFramework/Actor.h"
#include "ColorTransferTypes.h"

#include "ColorButton.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class AColorMovingFloor;
class UNiagaraSystem;
class UStaticMeshComponent;

/**
 * 颜料池按钮：射入的颜料按量混合改变自身颜色，池满且配比落在 TargetMix 容差内后 FuseDelay 秒碎裂，碎裂时触发一块移动地板。
 * 碎裂 RespawnDelay 秒后以开局状态重生，可再次触发。
 *
 * 只认射线颜色，不认射手队伍：第一关要求红色射断，蓝方要先把红色传过来才能做到。
 * 池满后继续注入会按比例挤出已混匀的旧颜料，因此总能被重新调色。
 * 混色只在服务器上结算，客户端向复制来的混合色渐变；颜色通过网格材质的 Color 向量参数实现，材质需带该参数。
 * 子类通过 AuthShouldPrime / AuthOnBroken 改写触发条件与碎裂后果。
 */
UCLASS(MinimalAPI)
class AColorButton : public AActor, public IColorShotReceiver
{
	GENERATED_BODY()

public:
	UE_API AColorButton();

	//~Begin IColorShotReceiver Interface
	UE_API virtual void ReceiveColorShot(const FColorShotHit& Shot) override;
	//~End IColorShotReceiver Interface

	//~Begin AActor Interface
	UE_API virtual void Tick(float DeltaSeconds) override;
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

protected:
	//~Begin AActor Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End AActor Interface

	/** 注入这一枪后是否进入倒计时，仅在服务器上调用；默认判定池满且配比符合 TargetMix */
	UE_API virtual bool AuthShouldPrime(const FColorShotHit& Shot);

	/** 碎裂后的后果，仅在服务器上调用；默认触发 TriggeredFloor */
	UE_API virtual void AuthOnBroken();

	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** 开局装满池子的颜料；None 表示空池，首次注入前保持网格材质原有颜色 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer")
	EColorPigment DisplayPigment = EColorPigment::None;

	/** 池子容量，满后新注入按比例挤出旧颜料 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "1.0"))
	float PoolCapacity = 20.0f;

	/** 触发所需的颜料配比，Amount 为相对占比，未列出的颜料要求占比为 0 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer")
	TArray<FColorPigmentLayer> TargetMix = {{EColorPigment::Red, 1.0f}};

	/** 每种颜料实际占比与目标占比允许的偏差 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MixTolerance = 0.1f;

	/** 客户端显示色追向混合色的速度 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.1"))
	float ColorInterpSpeed = 4.0f;

	/** 进入倒计时到碎裂的间隔 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.0", ForceUnits = "s"))
	float FuseDelay = 2.0f;

	/** 碎裂到重生的间隔，0 表示不重生 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.0", ForceUnits = "s"))
	float RespawnDelay = 5.0f;

	UPROPERTY(EditInstanceOnly, Category = "Yan|ColorTransfer")
	TObjectPtr<AColorMovingFloor> TriggeredFloor;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	TSoftObjectPtr<UNiagaraSystem> ExplosionEffect;

private:
	void AuthBreak();

	void AuthRespawn();

	// 池子恢复为开局状态：DisplayPigment 装满或空池
	void AuthResetPool();

	// 注入颜料，超出容量的部分按各颜料占比等比溢出
	void AuthInjectPigment(EColorPigment Pigment, float Amount);

	bool IsPoolMatchingTarget() const;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExplode(FVector_NetQuantize Location);

	UFUNCTION()
	void OnRep_Visual();

	UFUNCTION()
	void OnRep_PoolColor();

	// 池内各颜料的量，每种颜料一项，只在服务器上维护
	TArray<FColorPigmentLayer> PoolContents;

	// 池内颜料的混合色；透明表示空池
	UPROPERTY(ReplicatedUsing = OnRep_PoolColor)
	FLinearColor PoolColor = FLinearColor::Transparent;

	// 当前写入材质的颜色，逐帧追向 PoolColor
	FLinearColor DisplayedColor = FLinearColor::Transparent;

	// 开局时的显示色，重生时直接恢复；透明表示材质没有可读的颜色
	FLinearColor InitialDisplayedColor = FLinearColor::Transparent;

	// 已被射中、正在倒计时，此后不再响应射线
	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	bool bPrimed = false;

	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	bool bBroken = false;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> LoadedExplosionEffect;

	FTimerHandle FuseTimer;

	FTimerHandle RespawnTimer;
};

#undef UE_API
