#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "YanGrappleAuthorityComponent.generated.h"

#define UE_API YANGAMEMOVER_API

class APawn;
class UMoverComponent;
struct FYanCharacterInputs;

/**
 * 钩锁钩住 Pawn 时的服务器裁决器：读本 Pawn 上传的牵引意图，认定目标后向其下发牵引。
 *
 * 之所以由 game thread 的组件承担，而非在 sim 侧就地完成：跨 Pawn 施力只能经
 * UNetworkPhysicsComponent 的 EnqueueScheduledAction_External 下发，那是 game-thread 接口，
 * 而 sim 侧的 Transition 与运动模式运行于物理线程，在那里访问其他 Actor 并不安全。
 *
 * 与 UYanClinchAuthorityComponent 的关键差别在于下发节奏：贴身抓取的落位点一次算定，
 * 故只在意图的上升沿裁决一次；钩锁的汇合点每帧都在动，须按刷新间隔反复下发，
 * 每次带上当时的汇合点。目标则只在上升沿认定一次并沿用，中途不重认——
 * 逐次认定会在两人交错而过时把牵引转移到旁人身上。
 *
 * 发起者自身的牵引不经本组件：那一侧走输入包与 UChaosGrapplingMode，两端各自预测。
 */
UCLASS(MinimalAPI, Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UYanGrappleAuthorityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UE_API UYanGrappleAuthorityComponent();

	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~End UActorComponent Interface

protected:
	/**
	 * 目标认定的搜索半径（cm）。取值应容得下发起端与服务器之间的位置差：
	 * 发起端看到的目标位置是插值外推的结果，与服务器上的真值本就不同。
	 */
	UPROPERTY(EditAnywhere, Category = "Grappling", meta = (ClampMin = "0", ForceUnits = "cm"))
	float VerifyRadius = 300.0f;

	/** 朝汇合点的主动拉力加速度（cm/s^2），与 UChaosGrapplingMode 上的同名参数取齐才对称 */
	UPROPERTY(EditAnywhere, Category = "Grappling", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float PullAcceleration = 3000.0f;

	/** 被钩者的合速度上限（cm/s） */
	UPROPERTY(EditAnywhere, Category = "Grappling", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxPullSpeed = 2500.0f;

	/** 距汇合点在此以内即不再施力（cm），避免双方隔着汇合点反复对冲 */
	UPROPERTY(EditAnywhere, Category = "Grappling", meta = (ClampMin = "0", ForceUnits = "cm"))
	float StopDistance = 80.0f;

	/**
	 * 牵引的刷新间隔（秒）。间隔越短汇合点越跟手，代价是等比例增加的 sim action 下发量；
	 * 单次牵引的时长取其两倍，前后两次因而始终有重叠，衔接处不会出现无牵引的空档。
	 */
	UPROPERTY(EditAnywhere, Category = "Grappling", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float RefreshIntervalSeconds = 0.1f;

private:
	// 认定目标并下发一次牵引，失败时只记日志
	UE_API void ResolveAndPull(const FYanCharacterInputs& YanInputs);

	// 在给定位置附近找最近的、带 MoverComponent 的 Pawn（排除自身）
	UE_API APawn* FindPawnNear(const FVector& Location) const;

	// 本轮钩锁已认定的目标：意图落回 false 时清空，下一次钩中方可重新认定
	UPROPERTY(Transient)
	TWeakObjectPtr<UMoverComponent> ResolvedTargetMoverComp;

	// 距上次下发的时间（秒），累加到刷新间隔即下发一次。
	// 目标认定与牵引下发共用同一节流，认定失败不会退化成逐帧的碰撞查询
	float TimeSinceLastPull = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMoverComponent> CachedMoverComp;
};

#undef UE_API
