#pragma once

#include "CoreMinimal.h"
#include "LayeredMove.h"
#include "LayeredMoveBase.h"
#include "InstantMovementEffect.h"
#include "MoverLayeredMove/YanBlinkToMove.h"

#include "YanRadialAttractionMove.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 异步径向牵引：默认叠加带距离衰减的速度；收拢策略覆盖速度，进入内圈后复用 Blink 轨迹跟随中心。
 * 中心只使用随 Authority action 下发的坐标、速度和模拟时间，不在物理线程读取 Actor。
 * 同一来源通过 FYanRadialAttractionUpdateEffect 刷新，进入速度和收拢进度随 LayeredMove 同步及回滚。
 */
USTRUCT(BlueprintType)
struct FYanLayeredMove_RadialAttraction : public FLayeredMoveBase
{
	GENERATED_BODY()

	UE_API FYanLayeredMove_RadialAttraction();

	virtual ~FYanLayeredMove_RadialAttraction() = default;

	/** 引力中心（世界空间） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction")
	FVector Location = FVector::ZeroVector;

	/** 作用半径（cm）：未捕获目标超出此距离无牵引；内圈捕获后不再检查距离。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction", meta = (ClampMin = "0", ForceUnits = "cm"))
	float Radius = 500.f;

	/** 牵引速度（cm/s）：默认向外衰减；收拢策略在外圈使用恒速。 */
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

	/** 来源标识，仅由服务器分配；0 保留给不刷新状态的径向牵引。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction")
	int32 SourceId = 0;

	UPROPERTY()
	uint32 UpdateSequence = 0;

	/** 中心快照所对应的目标 Mover 模拟时间（毫秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction")
	double SnapshotSimTimeMs = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction")
	FVector CenterVelocity = FVector::ZeroVector;

	/** 覆盖式恒速吸引；进入内圈后持续锁定球心，直到来源显式取消。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction")
	bool bCaptureToCenter = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction", meta = (ClampMin = "0"))
	float InnerRadius = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction", meta = (ClampMin = "0", ClampMax = "1"))
	float EntryVelocityDamping = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction", meta = (ClampMin = "1", ForceUnits = "ms"))
	float CaptureDurationMs = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction", meta = (ClampMin = "0.01"))
	float CaptureEaseExponent = 0.5f;

	/** 在模拟线程刷新参数和存续时间；Radius <= 0 表示取消，序号过旧则忽略。 */
	UE_API void Refresh(const FYanLayeredMove_RadialAttraction& Settings, double CurrentSimTimeMs);

	//~Begin FLayeredMoveBase Interface
	virtual bool SupportsAsync() const override { return true; }
	UE_API virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const override;
	UE_API virtual void GetGameplayTags(FGameplayTagContainer& InOutTags) const override;

	UE_API virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	UE_API virtual bool GenerateMove_Async(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;

	UE_API virtual FLayeredMoveBase* Clone() const override;
	UE_API virtual void NetSerialize(FArchive& Ar) override;
	UE_API virtual UScriptStruct* GetScriptStruct() const override;
	UE_API virtual FString ToSimpleString() const override;
	UE_API virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
	//~End FLayeredMoveBase Interface

private:
	friend struct FYanRadialAttractionUpdateEffect;

	// 刷新动作只携带调参；进入速度与 Blink 进度仅随模拟状态同步。
	void SerializeSettings(FArchive& Ar);

	// 只在进入时采样，避免把重力或被阻挡后的速度累积进残余切向速度。
	UPROPERTY()
	FVector RetainedVelocity = FVector::ZeroVector;

	UPROPERTY()
	bool bHasEntryVelocity = false;

	UPROPERTY()
	bool bCaptured = false;

	// 在以球心为原点的平移坐标系里复用 Blink，球移动时不重启收拢进度。
	UPROPERTY()
	FYanLayeredMove_BlinkTo CaptureMove;
};

/** Authority 刷新动作：更新同一来源的活动牵引；首次下发时才创建 LayeredMove。 */
USTRUCT()
struct FYanRadialAttractionUpdateEffect : public FInstantMovementEffect
{
	GENERATED_BODY()

	UPROPERTY()
	FYanLayeredMove_RadialAttraction Settings;

	//~Begin FInstantMovementEffect Interface
	UE_API virtual bool ApplyMovementEffect_Async(FApplyMovementEffectParams_Async& ApplyEffectParams, FMoverSyncState& OutputState) override;
	UE_API virtual FInstantMovementEffect* Clone() const override;
	UE_API virtual void NetSerialize(FArchive& Ar) override;
	UE_API virtual UScriptStruct* GetScriptStruct() const override;
	UE_API virtual FString ToSimpleString() const override;
	//~End FInstantMovementEffect Interface
};

template <>
struct TStructOpsTypeTraits<FYanRadialAttractionUpdateEffect> : public TStructOpsTypeTraitsBase2<FYanRadialAttractionUpdateEffect>
{
	enum
	{
		WithCopy = true
	};
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
