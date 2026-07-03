// Copyright Chronicler.

#pragma once

#include "CoreMinimal.h"

#include "CameraPenetrationAvoidanceFeeler.generated.h"

/**
 * 相机穿透规避用的探针射线（Feeler）配置。
 */
USTRUCT()
struct FCameraPenetrationAvoidanceFeeler
{
	GENERATED_BODY()

	/** 相对主射线方向的旋转偏移（FRotator）。 */
	UPROPERTY(EditAnywhere, Category=PenetrationAvoidanceFeeler)
	FRotator AdjustmentRot;

	/** 击中 World 几何时对最终相机位置的权重贡献。 */
	UPROPERTY(EditAnywhere, Category=PenetrationAvoidanceFeeler)
	float WorldWeight;

	/** 击中 APawn 时的权重（0 表示不对 Pawn 做碰撞）。 */
	UPROPERTY(EditAnywhere, Category=PenetrationAvoidanceFeeler)
	float PawnWeight;

	/** Sweep 使用的 Sphere 半径（Extent）。 */
	UPROPERTY(EditAnywhere, Category=PenetrationAvoidanceFeeler)
	float Extent;

	/** 上一帧未命中时，本 Feeler 的最小 Trace 间隔（帧）。 */
	UPROPERTY(EditAnywhere, Category=PenetrationAvoidanceFeeler)
	int32 TraceInterval;

	/** 距离下次允许 Trace 的剩余帧数（运行时）。 */
	UPROPERTY(transient)
	int32 FramesUntilNextTrace;


	FCameraPenetrationAvoidanceFeeler()
		: AdjustmentRot(ForceInit)
		, WorldWeight(0)
		, PawnWeight(0)
		, Extent(0)
		, TraceInterval(0)
		, FramesUntilNextTrace(0)
	{
	}

	FCameraPenetrationAvoidanceFeeler(const FRotator& InAdjustmentRot,
									const float& InWorldWeight, 
									const float& InPawnWeight, 
									const float& InExtent, 
									const int32& InTraceInterval = 0, 
									const int32& InFramesUntilNextTrace = 0)
		: AdjustmentRot(InAdjustmentRot)
		, WorldWeight(InWorldWeight)
		, PawnWeight(InPawnWeight)
		, Extent(InExtent)
		, TraceInterval(InTraceInterval)
		, FramesUntilNextTrace(InFramesUntilNextTrace)
	{
	}
};
