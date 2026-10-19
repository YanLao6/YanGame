#pragma once

#include "Actors/ColorChargeOrb.h"
#include "Engine/NetSerialization.h"

#include "ColorAttractOrb.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class UNiagaraSystem;
class USoundBase;
class UMoverComponent;

/**
 * 蓝方吸引球：蓄力期间挂在手上、松手后脱手进入操控阶段，两个阶段都会吸引周围的 Pawn（施法者除外）。
 *
 * 吸引半径与每秒注入的蓝色随蓄力进度插值；进入 InnerRadius 后锁定并短时收拢到球心，跟随直到球销毁。
 * 操控阶段以施法者的相机为坐标系：相机中心射线上有一个目标点，左键沿射线推远、右键拉近，
 * 鼠标转动相机即带着目标点上下左右移动；球以不超过 MoveSpeed 的速度追向目标点；左 Ctrl 放弃操控并直接销毁球。
 * 相机射线与油门由施法者客户端每帧上报（相机只存在于本地端）。
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
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

protected:
	//~Begin AActor Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End AActor Interface

	/** 蓄力进度 0 与 1 时的吸引半径（cm） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	FVector2D AttractRadiusRange = FVector2D(100.0f, 500.0f);

	/** 蓄力进度 0 与 1 时，被吸引者每秒受到的蓝色 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	FVector2D PigmentPerSecondRange = FVector2D(1.0f, 5.0f);

	/** 此半径内触发到球心的短时收拢 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.0"))
	float InnerRadius = 50.0f;

	/** 捕获前在外圈被吸向球心的速度（cm/s）；捕获后按球心位置跟随。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.0"))
	float PullSpeed = 600.0f;

	/** 进入吸引半径时抹去的非径向速度比例：1 为全部抹去、直线吸向球心；残余部分在半径内保持，直到离开 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EntryVelocityDamping = 1.0f;

	/** 吸引参数刷新间隔；连续位移由目标 ChaosMover 的物理模拟步解算。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.01", ForceUnits = "s"))
	float AttractionRefreshInterval = 0.05f;

	/** 进入内圈后到球心的位移时长（毫秒）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer", Meta = (ClampMin = "1.0", ForceUnits = "ms"))
	float InnerBlinkDurationMs = 150.0f;

	/** 内圈位移的时间重映射指数，小于 1 起步快、末端收缓。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.01"))
	float InnerBlinkEaseExponent = 0.5f;

	/** 操控阶段球追向目标点的速度上限（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.0"))
	float MoveSpeed = 100.0f;

	/** 左右键沿相机射线推拉目标点的速度（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.0"))
	float PushSpeed = 100.0f;

	/** 目标点距相机的范围（cm） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	FVector2D AimDistanceRange = FVector2D(100.0f, 5000.0f);

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

	// 每帧上报，丢包由下一帧覆盖，故不走 Reliable
	UFUNCTION(Server, Unreliable)
	void ServerUpdateControl(FVector_NetQuantize InAimStart, FVector_NetQuantizeNormal InAimDirection, int8 InThrottle);

	// 施法者主动放弃操控：直接销毁，不播放引爆表现
	UFUNCTION(Server, Reliable)
	void ServerCancelControl();

	// 服务器：按相机射线与油门推进目标点。
	void AuthMoveTowardsAim(float DeltaSeconds);

	// 服务器：定时刷新目标的牵引参数；物理线程持有并推进牵引状态。
	void AuthRefreshAttraction();

	// 服务器：取消该球对指定 Mover 的牵引。
	void AuthReleaseAttraction(UMoverComponent* Mover);

	// 服务器：每秒给范围内的 Pawn 注入蓝色
	void AuthInjectTick();

	// 施法者客户端：上报相机射线与鼠标左右键，按下左 Ctrl 则请求销毁
	void SendLocalControl();

	float GetAttractRadius() const;

	TSet<APawn*> GatherAttractedPawns() const;

	UPROPERTY(Replicated)
	bool bControlled = false;

	// 以下仅服务器：+1 推远、-1 拉近
	int8 Throttle = 0;

	FVector AimStart = FVector::ZeroVector;

	FVector AimDirection = FVector::ZeroVector;

	// 目标点沿相机射线的距离，收到首个上报时按球的当前位置初始化，小于 0 表示尚未初始化
	float AimDistance = -1.0f;

	// 保存球存续期间接触过的有效目标，持续刷新并在销毁时取消；捕获状态仅由 LayeredMove 持有。
	TSet<TWeakObjectPtr<UMoverComponent>> AttractedMovers;

	FVector LastAttractionLocation = FVector::ZeroVector;
	double LastAttractionSampleTime = -1.0;
	uint32 AttractionUpdateSequence = 0;

	FTimerHandle InjectTimer;
	FTimerHandle AttractionTimer;

	// 非专用服务器：提前加载并持有，避免在爆炸那一帧同步加载卡顿
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> LoadedDetonateEffect;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LoadedDetonateSound;
};

#undef UE_API
