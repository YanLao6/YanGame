#pragma once

#include "CoreMinimal.h"
#include "LayeredMove.h"
#include "LayeredMoveBase.h"

#include "YanBlinkToMove.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 瞬移：在固定时长内把角色从起点送到终点，抵达即停。
 *
 * 与冲量类效果（FLayeredMove_LinearVelocity、FChaosCharacterApplyVelocityEffect）的根本区别在驱动方式：
 * 本效果每帧解算「此刻应处的位置」，速度由位置差反推得到。终点不再前移时速度自然归零，
 * 既不需要减速曲线，也不会出现冲量到期后速度残留、被摩擦与重力接管而滑出去的现象。
 * 位移仍逐帧交由移动模式执行，故撞墙会被挡住而非穿透。
 *
 * 引擎自带的 FLayeredMove_MoveTo 算的是同一件事，但它只实现了同步的 GenerateMove，
 * 而 ChaosMover 恒为异步仿真，只执行 SupportsAsync() 为真的 LayeredMove——直接拿来用会被静默丢弃。
 * 另两处不同：当前位置取自 SyncState 而非 AActor（物理线程访问 AActor 不安全）；
 * 时间重映射用指数公式而非 UCurveFloat（曲线对象不是线程安全的，在物理线程上取值本就不该做）。
 *
 * 起点与终点均在发起时确定并随效果一同下发，物理线程不再重算，
 * 客户端预测与服务器重模拟据此落在同一条轨迹上。
 */
USTRUCT(BlueprintType)
struct FYanLayeredMove_BlinkTo : public FLayeredMoveBase
{
	GENERATED_BODY()

	UE_API FYanLayeredMove_BlinkTo();

	virtual ~FYanLayeredMove_BlinkTo() = default;

	/** 轨迹起点（世界空间），取发起帧的仿真位置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector StartLocation = FVector::ZeroVector;

	/** 轨迹终点（世界空间）。须由发起方预先做碰撞收敛，本效果不校验其可站立性 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector TargetLocation = FVector::ZeroVector;

	/** 首帧强制切换的移动模式（NAME_None 则维持当前模式）。仅首帧生效，中途模式变化需另行入队 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FName ForceMovementMode = NAME_None;

	/**
	 * 时间重映射指数，作用于 [0,1] 的进度：小于 1 起步快、末端收得缓（爆发感），
	 * 等于 1 为匀速，大于 1 则起步慢。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = "0.01"))
	float EaseExponent = 0.5f;

	/**
	 * 限速：被阻挡而落后于期望位置时，不允许本步速度超过理论值。
	 * 关闭则会因位置差持续拉大而贴墙加压，产生异常推挤。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bRestrictSpeedToExpected = true;

	//~Begin FLayeredMoveBase Interface
	virtual bool SupportsAsync() const override { return true; }

	UE_API virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	UE_API virtual bool GenerateMove_Async(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;

	UE_API virtual FLayeredMoveBase* Clone() const override;
	UE_API virtual void NetSerialize(FArchive& Ar) override;
	UE_API virtual UScriptStruct* GetScriptStruct() const override;
	UE_API virtual FString ToSimpleString() const override;
	UE_API virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
	//~End FLayeredMoveBase Interface

private:
	// 把线性进度重映射为位移进度，两端须得到完全一致的结果，故只用纯函数
	UE_API float ApplyTimeMapping(float LinearFraction) const;
};

template <>
struct TStructOpsTypeTraits<FYanLayeredMove_BlinkTo> : public TStructOpsTypeTraitsBase2<FYanLayeredMove_BlinkTo>
{
	enum
	{
		WithCopy = true
	};
};

#undef UE_API
