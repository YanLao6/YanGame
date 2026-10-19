#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "YanAnimSlotLibrary.generated.h"

#define UE_API YANGAMEPLAY_API

class USkeleton;

/**
 * 骨骼动画槽（Slot）与槽组（Slot Group）的脚本入口。
 *
 * 引擎只在骨骼编辑器的 Anim Slot Manager 里提供槽组编辑，USkeleton 的对应接口未暴露给
 * 蓝图 / Python / AngelScript。蒙太奇的槽组决定 Montage_Play 会停掉哪些正在播放的蒙太奇
 * （同组互停），因此需要按组隔离的蒙太奇必须能在自动化流程里登记到独立的组。
 */
UCLASS(MinimalAPI)
class UYanAnimSlotLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 确保骨骼上存在指定槽并归入指定槽组；组不存在则创建。仅在编辑器下生效。
	 * @return 骨骼数据发生了变化（已标脏，需由调用方保存）
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Animation", meta = (DevelopmentOnly))
	static UE_API bool EnsureSlotInGroup(USkeleton* Skeleton, FName SlotName, FName GroupName);

	/** 读取骨骼上某个槽所属的槽组名；未登记的槽返回 DefaultGroup */
	UFUNCTION(BlueprintPure, Category = "Yan|Animation")
	static UE_API FName GetSlotGroupName(const USkeleton* Skeleton, FName SlotName);
};

#undef UE_API
