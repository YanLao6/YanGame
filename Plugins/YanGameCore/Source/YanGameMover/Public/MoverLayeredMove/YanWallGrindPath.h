#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

#define UE_API YANGAMEMOVER_API

/**
 * 贴墙推进的路径求值，全部为纯函数。
 *
 * 独立于 Mover 与 GAS 存在：不读 Actor、不读黑板、不依赖帧率，同一组入参在任何线程、
 * 任何端、任何一次重演中必得同一结果。推进双方正是靠共用本组函数与同一条折线，
 * 在不读取对方位置的前提下保持相对位置守恒——对方的位置在本地是插值外推的结果，
 * 与权威值本就不同，读它必然分歧。
 *
 * 运动模型为沿折线的匀减速：s(t) = s0·t − ½·a·t²，
 * 推进在「速度耗尽」与「走完折线」两者先到时结束。
 */
namespace YanWallGrindPath
{
	/** 折线总弧长（cm）。点数不足 2 时返回 0 */
	UE_API double ComputeTotalLength(TConstArrayView<FVector> Path);

	/** 匀减速下的自然停止距离 s0²/(2a)。减速度非正视为不减速，返回 double 上限 */
	UE_API double ComputeStopDistance(double EntrySpeed, double Deceleration);

	/** 推进总时长（秒）：取「速度耗尽」与「走完折线」两者先到者 */
	UE_API double ComputeDuration(double EntrySpeed, double Deceleration, double TotalLength);

	/** 起滑后 ElapsedSeconds 时刻已行进的弧长，钳制在 [0, min(停止距离, 总长)] */
	UE_API double ComputeTraveledDistance(double EntrySpeed, double Deceleration, double ElapsedSeconds, double TotalLength);

	/**
	 * 弧长 → 世界位置与单位切线。Distance 超出两端时钳制到首/末点。
	 * 点数不足 2 时取首点、切线取零向量并返回 false。
	 */
	UE_API bool EvaluateAtDistance(TConstArrayView<FVector> Path, double Distance, FVector& OutLocation, FVector& OutTangent);

	/**
	 * 把路径坐标系下的常量偏移换算到世界空间：X 沿切线、Y 沿外向、Z 沿世界上。
	 *
	 * 偏移之所以定义在随切线旋转的坐标系里而非世界空间：墙面转弯时，
	 * 世界空间的常量偏移会让发起方从被推方身后滑到侧面。
	 * 外向取切线与世界上的叉积（水平面内垂直于路径）；切线接近竖直时退化，
	 * 依次改用固定的世界轴叉积兜底，保持纯函数性。
	 */
	UE_API FVector TransformFrameOffset(const FVector& Tangent, const FVector& FrameOffset);
}

#undef UE_API
