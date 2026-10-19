#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "YanMoverDataModelTypes.h"
#include "MoverMode/Fatigue/YanFatigueState.h"

#include "MoverDataLibrary.generated.h"

#define UE_API YANGAMEMOVER_API

class UMoverComponent;

/** Mover 输入包与同步状态的数据桥接。 */
UCLASS(MinimalAPI)
class UMoverDataLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 从 FMoverDataCollection 读取 FMoverDefaultSyncState（存在则 true 并写入 Out）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Data")
	static UE_API bool GetDefaultSyncState(const FMoverDataCollection& Collection, FMoverDefaultSyncState& OutSyncState);

	/** 从输入数据集合读取默认角色输入，存在时写入 OutInputs。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Data")
	static UE_API bool GetDefaultInputs(const FMoverDataCollection& Collection, FCharacterDefaultInputs& OutInputs);

	/** 从输入命令包读取项目自定义输入（存在则 true 并写入 Out）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Data")
	static UE_API bool GetYanCharacterInputs(const FMoverDataCollection& Collection, FYanCharacterInputs& OutInputs);

	/**
	 * 从输入命令包获取（或创建）FCharacterDefaultInputs 副本。
	 * AS 无法直接调用模板方法 FindOrAddMutableDataByType，通过此函数桥接。
	 * 修改后须调用 CommitDefaultInputs 写回；前一个 ability 写入的字段不会丢失。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|AbilityInput")
	static UE_API FCharacterDefaultInputs GetOrAddDefaultInputs(UPARAM(ref) FMoverInputCmdContext& InputCmd);

	/** 将修改后的 FCharacterDefaultInputs 写回输入命令包。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|AbilityInput")
	static UE_API void CommitDefaultInputs(UPARAM(ref) FMoverInputCmdContext& InputCmd, const FCharacterDefaultInputs& Inputs);

	/**
	 * 从输入命令包获取（或创建）FYanCharacterInputs 副本。
	 * 修改后须调用 CommitYanInputs 写回。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|AbilityInput")
	static UE_API FYanCharacterInputs GetOrAddYanInputs(UPARAM(ref) FMoverInputCmdContext& InputCmd);

	/** 将修改后的 FYanCharacterInputs 写回输入命令包。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|AbilityInput")
	static UE_API void CommitYanInputs(UPARAM(ref) FMoverInputCmdContext& InputCmd, const FYanCharacterInputs& Inputs);

	/**
	 * 读取 Mover 最近一次同步状态里某疲劳通道的记录，供 UI 显示冷却。
	 * 通道不存在即已完全恢复，返回 false；本地受控端读到的是预测值。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Fatigue")
	static UE_API bool GetFatigueEntry(const UMoverComponent* MoverComp, EYanFatigueChannel Channel, FYanFatigueEntry& OutEntry);
};

#undef UE_API
