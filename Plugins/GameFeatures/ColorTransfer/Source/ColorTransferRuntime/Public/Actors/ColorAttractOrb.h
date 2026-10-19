#pragma once

#include "Actors/ColorChargeOrb.h"
#include "Engine/NetSerialization.h"

#include "ColorAttractOrb.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class UNiagaraSystem;
class USoundBase;

/**
 * 蓝方吸引球：蓄力期间挂在手上、松手后脱手进入操控阶段，两个阶段都会吸引周围的 Pawn（施法者除外）。
 *
 * 吸引半径与每秒注入的蓝色随蓄力进度插值；进入 InnerRadius 后锁定，以受限速度飞向球心并跟随直到球销毁。
 * 操控阶段由父类从服务器 Pawn 视点和 Mover 输入中的 ControlRotation 推进目标点。
 * 左键推远、右键拉近，客户端只在推拉状态变化时提交意图；左 Ctrl 放弃操控并直接销毁球。
 * 吸引参数由服务器下发，目标移动由 ChaosMover 模拟；注色与命中融合球靶子只在服务器上结算。
 */
UCLASS(MinimalAPI)
class AColorAttractOrb : public AColorChargeOrb
{
	GENERATED_BODY()

public:
	UE_API AColorAttractOrb();

	/** 仅在服务器上调用：脱手并开始操控，持续 Duration 秒后自毁。 */
	UE_API void AuthLaunch(float Duration);

	/** 仅在服务器上调用：在 Location 播放爆炸并销毁自身，蓄力与操控阶段均可。 */
	UE_API void AuthDetonate(const FVector& Location);

	/** 操控阶段结束（到时或被销毁）时广播，仅服务器。蓄力阶段被销毁同样广播。 */
	FSimpleMulticastDelegate OnControlFinished;

	//~Begin AActor Interface
	UE_API virtual void Tick(float DeltaSeconds) override;
	//~End AActor Interface

protected:
	//~Begin AActor Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End AActor Interface

	//~Begin AYanAttractionActor Interface
	UE_API virtual float GetAttractRadius_Implementation() const override;
	UE_API virtual void SendLocalControl() override;
	//~End AYanAttractionActor Interface

	/** 蓄力进度 0 与 1 时的吸引半径（cm） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	FVector2D AttractRadiusRange = FVector2D(100.0f, 500.0f);

	/** 蓄力进度 0 与 1 时，被吸引者每秒受到的蓝色 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	FVector2D PigmentPerSecondRange = FVector2D(1.0f, 5.0f);

	/** 检测融合球靶子的半径（cm） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer", Meta = (ClampMin = "1.0"))
	float TargetHitRadius = 30.0f;

	/** 被引爆时的特效 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer|Detonate")
	TSoftObjectPtr<UNiagaraSystem> DetonateEffect;

	/** 被引爆时的音效 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer|Detonate")
	TSoftObjectPtr<USoundBase> DetonateSound;

private:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastDetonate(FVector_NetQuantize Location);

	// 施法者主动放弃操控：直接销毁，不播放引爆表现
	UFUNCTION(Server, Reliable)
	void ServerCancelControl();

	// 服务器：每秒给范围内的 Pawn 注入蓝色
	void AuthInjectTick();

	FTimerHandle InjectTimer;

	// 非专用服务器：提前加载并持有，避免在爆炸那一帧同步加载卡顿
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> LoadedDetonateEffect;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LoadedDetonateSound;
};

#undef UE_API
