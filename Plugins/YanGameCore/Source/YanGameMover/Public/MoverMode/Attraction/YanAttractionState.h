#pragma once

#include "InstantMovementEffect.h"
#include "MoverSimulationTypes.h"

#include "YanAttractionState.generated.h"

#define UE_API YANGAMEMOVER_API

/** 外部来源的牵引意图。坐标与速度由服务器采样，模拟线程不读取来源 Actor。 */
USTRUCT(BlueprintType)
struct FYanAttractionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction")
	int32 SourceId = 0;

	UPROPERTY()
	uint32 UpdateSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction")
	FVector CenterVelocity = FVector::ZeroVector;

	UPROPERTY()
	double SnapshotSimTimeMs = 0.0;

	/** 未锁定时的作用半径；不大于零表示释放该来源。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction", meta = (ClampMin = "0", ForceUnits = "cm"))
	float Radius = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction", meta = (ClampMin = "0", ForceUnits = "cm"))
	float InnerRadius = 50.0f;

	/** 外圈与内圈共用的期望速度上限，仍受角色飞行速度上限约束。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float PullSpeed = 600.0f;

	/** 未捕获来源的刷新租期，同时限制旧中心速度的外推时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Attraction", meta = (ClampMin = "1", ForceUnits = "ms"))
	float DurationMs = 150.0f;

	/** 将中心快照外推至模拟时间；刷新中断后停止外推。 */
	UE_API FVector GetCenter(double SimTimeMs) const;
	UE_API void NetSerialize(FArchive& Ar);
	UE_API bool operator==(const FYanAttractionSettings& Other) const;
};

USTRUCT()
struct FYanAttractionSource
{
	GENERATED_BODY()

	UPROPERTY()
	FYanAttractionSettings Settings;

	UPROPERTY()
	double ExpirationSimTimeMs = 0.0;

	UPROPERTY()
	bool bCaptured = false;

	/** 捕获后只接受来源释放；捕获前允许离开外圈或租期结束。 */
	UE_API bool CanControl(const FVector& PawnLocation, double SimTimeMs) const;
};

/** 牵引来源与锁定状态的唯一模拟记录，按值复制并随 SyncState 回滚。 */
USTRUCT()
struct FYanAttractionState : public FMoverDataStructBase
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FYanAttractionSource> Sources;

	UPROPERTY()
	int32 ActiveSourceId = 0;

	/** 保留有效的当前来源；无当前来源时按 SourceId 确定性选择。 */
	UE_API const FYanAttractionSource* SelectSource(const FVector& PawnLocation, double SimTimeMs) const;
	UE_API bool Refresh(const FYanAttractionSettings& Settings, double SimTimeMs);
	UE_API void UpdateCapture(const FVector& PawnLocation, double SimTimeMs);
	UE_API void RemoveExpired(double SimTimeMs);

	//~Begin FMoverDataStructBase Interface
	UE_API virtual FMoverDataStructBase* Clone() const override;
	UE_API virtual UScriptStruct* GetScriptStruct() const override;
	UE_API virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	UE_API virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	UE_API virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
	UE_API virtual void Merge(const FMoverDataStructBase& From) override;
	//~End FMoverDataStructBase Interface
};

/** Authority action 只更新控制状态，位移由 UChaosAttractionMode 逐物理步执行。 */
USTRUCT()
struct FYanRadialAttractionUpdateEffect : public FInstantMovementEffect
{
	GENERATED_BODY()

	UPROPERTY()
	FYanAttractionSettings Settings;

	//~Begin FInstantMovementEffect Interface
	UE_API virtual bool ApplyMovementEffect_Async(FApplyMovementEffectParams_Async& ApplyEffectParams, FMoverSyncState& OutputState) override;
	UE_API virtual FInstantMovementEffect* Clone() const override;
	UE_API virtual void NetSerialize(FArchive& Ar) override;
	UE_API virtual UScriptStruct* GetScriptStruct() const override;
	UE_API virtual FString ToSimpleString() const override;
	//~End FInstantMovementEffect Interface
};

template <>
struct TStructOpsTypeTraits<FYanAttractionState> : public TStructOpsTypeTraitsBase2<FYanAttractionState>
{
	enum { WithNetSerializer = true, WithCopy = true };
};

template <>
struct TStructOpsTypeTraits<FYanRadialAttractionUpdateEffect> : public TStructOpsTypeTraitsBase2<FYanRadialAttractionUpdateEffect>
{
	enum { WithCopy = true };
};

#undef UE_API
