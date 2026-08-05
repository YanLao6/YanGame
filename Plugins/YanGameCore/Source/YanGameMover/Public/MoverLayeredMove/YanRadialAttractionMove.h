#pragma once

#include "CoreMinimal.h"
#include "LayeredMove.h"
#include "LayeredMoveBase.h"

#include "YanRadialAttractionMove.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 径向牵引：把角色朝一个世界坐标拉过去，越近拉得越狠，出了半径完全不起作用。
 *
 * 引擎自带的 FLayeredMove_RadialImpulse 算的是同一件事，但它只实现了同步的 GenerateMove，
 * 而 ChaosMover 恒为异步仿真，只执行 SupportsAsync() 为真的 LayeredMove——
 * 直接拿来用会被静默丢弃，一点效果都没有。本结构补上异步实现。
 *
 * 与引擎版的另一处不同：衰减用指数公式而非 UCurveFloat。曲线对象不是线程安全的，
 * 在物理线程上取值本就不该做；指数同样能表达「近处陡、远处缓」，还省掉一个资产。
 *
 * 引力中心以坐标而非 Actor 指针给出：物理线程访问 AActor 不安全。
 * 中心会移动的场合（如飞行中的苍）由发起方按刷新间隔反复下发，每次带上当时的位置。
 */
USTRUCT(BlueprintType)
struct FYanLayeredMove_RadialAttraction : public FLayeredMoveBase
{
	GENERATED_BODY()

	UE_API FYanLayeredMove_RadialAttraction();

	virtual ~FYanLayeredMove_RadialAttraction() = default;

	/** 引力中心（世界空间） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector Location = FVector::ZeroVector;

	/** 作用半径（cm）：超出此距离完全没有牵引，而非渐弱到零 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = "0", ForceUnits = "cm"))
	float Radius = 500.f;

	/** 引力中心处的牵引速度（cm/s），向外按衰减递减 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float Magnitude = 0.f;

	/**
	 * 衰减指数：1 为线性，大于 1 则牵引更集中在中心附近，小于 1 则边缘也拉得住。
	 * 作用于 (1 - 距离/半径)。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = "0.01"))
	float FalloffExponent = 1.f;

	/** true 推开、false 拉近 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bIsPush = false;

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
};

template <>
struct TStructOpsTypeTraits<FYanLayeredMove_RadialAttraction> : public TStructOpsTypeTraitsBase2<FYanLayeredMove_RadialAttraction>
{
	enum
	{
		WithCopy = true
	};
};

#undef UE_API
