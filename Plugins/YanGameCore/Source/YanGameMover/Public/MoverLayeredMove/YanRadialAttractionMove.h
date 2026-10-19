#pragma once

#include "CoreMinimal.h"
#include "LayeredMove.h"
#include "LayeredMoveBase.h"

#include "YanRadialAttractionMove.generated.h"

#define UE_API YANGAMEMOVER_API

/** 叠加带距离衰减的径向速度，用于允许自身移动的持续拉扯。 */
USTRUCT(BlueprintType)
struct FYanLayeredMove_RadialAttraction : public FLayeredMoveBase
{
	GENERATED_BODY()

	UE_API FYanLayeredMove_RadialAttraction();

	virtual ~FYanLayeredMove_RadialAttraction() = default;

	/** 引力中心（世界空间） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction")
	FVector Location = FVector::ZeroVector;

	/** 作用半径（cm），超出后无牵引。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction", meta = (ClampMin = "0", ForceUnits = "cm"))
	float Radius = 500.f;

	/** 牵引速度（cm/s），向外衰减。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float Magnitude = 0.f;

	/**
	 * 衰减指数：1 为线性，大于 1 则牵引更集中在中心附近，小于 1 则边缘也拉得住。
	 * 作用于 (1 - 距离/半径)。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction", meta = (ClampMin = "0.01"))
	float FalloffExponent = 1.f;

	/** true 推开、false 拉近 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction")
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
