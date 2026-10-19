#pragma once

#include "CoreMinimal.h"
#include "GameplayCueInterface.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "YanGameplayCueLibrary.generated.h"

#define UE_API YANGAMEABILITY_API

/**
 * 供 AngelScript / 蓝图调用的 GameplayCue 封装。
 *
 * 引擎的 ExecuteGameplayCueOnActor 对带 ASC 的目标只在服务器执行并经 ASC 复制，
 * 本地受控端要等一个 RTT 才看到效果。世界触发器（发射板、传送门）的位移已走 Mover 输入包预测，
 * 其 Cue 走「本地受控端立即执行 + 服务器多播给旁观者」双路触发，本库负责这两路的执行与去重。
 *
 * 去重靠 FGameplayCueParameters::RawMagnitude 上的预测标记：这两类 Cue 不使用 magnitude，
 * 借用该字段区分预测份与复制份。处理器须在开头调用 ShouldHandlePredictableCue。
 */
UCLASS(MinimalAPI)
class UYanGameplayCueLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 只在本机执行 Cue，不经 ASC 复制，参数上打预测标记 */
	UFUNCTION(BlueprintCallable, Category = "Yan|GameplayCue")
	static UE_API void ExecuteGameplayCuePredicted(AActor* Target, FGameplayTag GameplayCueTag, FGameplayCueParameters Parameters);

	/**
	 * 双路触发的 Cue 是否应在本机处理：目标 Pawn 由本机控制时，本机已执行过预测份，
	 * 服务器复制来的那份丢弃；其余情况（远端旁观、预测份本身）照常处理。
	 */
	UFUNCTION(BlueprintPure, Category = "Yan|GameplayCue")
	static UE_API bool ShouldHandlePredictableCue(const AActor* Target, const FGameplayCueParameters& Parameters);
};

#undef UE_API
