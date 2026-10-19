#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MoverSimulationTypes.h"
#include "MoveLibrary/MovementRecord.h"

#include "MoverSimulationLibrary.generated.h"

#define UE_API YANGAMEMOVER_API

/** Mover 仿真帧结果与移动记录的数据操作。 */
UCLASS(MinimalAPI)
class UMoverSimulationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 等价于 FMoverTickEndData::InitForNewFrame（清空 MovementEndState 与 MoveRecord）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|TickEnd")
	static UE_API void InitTickEndForNewFrame(UPARAM(ref) FMoverTickEndData& TickEndData);

	/** 调用 ResetToDefaults，清空 RemainingMs / NextModeName 等。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementModeTickEnd")
	static UE_API void ResetMovementModeTickEndState(UPARAM(ref) FMovementModeTickEndState& EndState);

	/** 清空 MovementRecord。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static UE_API void ResetMovementRecord(UPARAM(ref) FMovementRecord& Record);

	/** 构造一条 FMovementSubstep（成员在引擎侧未必对脚本暴露字段）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static UE_API FMovementSubstep MakeMovementSubstep(FName MoveName, FVector MoveDelta, bool bIsRelevant);

	/** 追加一条 substep。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static UE_API void AppendMovementSubstep(UPARAM(ref) FMovementRecord& Record, FMovementSubstep Substep);

	/** 拷贝一份 substep 列表（用于调试或遍历）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static UE_API TArray<FMovementSubstep> GetMovementRecordSubsteps(const FMovementRecord& Record);

	/** 锁定 relevancy 判定结果。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static UE_API void LockMovementRecordRelevancy(UPARAM(ref) FMovementRecord& Record, bool bLockValue);

	/** 解除 relevancy 锁定。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static UE_API void UnlockMovementRecordRelevancy(UPARAM(ref) FMovementRecord& Record);

	/** 调试字符串。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|MovementRecord")
	static UE_API FString MovementRecordToString(const FMovementRecord& Record);
};

#undef UE_API
