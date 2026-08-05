#pragma once

#include "CoreMinimal.h"

class APlayerController;

/**
 * 念力的屏幕投影口径。
 *
 * 目标选取与屏幕指示器必须用同一套投影，否则玩家会遇到「准心明明压住图标却选不中」——
 * 两者共用此处的函数，口径便无从漂移。
 */
namespace YanKinesis
{
	/**
	 * 本玩家分到的那块视口尺寸（像素）。
	 * 分屏下不等于整个视口：玩家的准心在自己那半屏的中心，而非整个窗口的中心。
	 * 无有效视口时返回 false。
	 */
	YANGAMEMOVER_API bool GetPlayerViewportSize(const APlayerController* OwningController, FVector2D& OutSize);

	/**
	 * 将世界锚点投影到玩家视口坐标。
	 *
	 * 先按视线前向剔除：视点背后的位置投影后会落到屏幕上的镜像点，
	 * 单凭投影函数的返回值不足以排除。
	 */
	YANGAMEMOVER_API bool ProjectAnchorToScreen(const APlayerController* OwningController, const FVector& ViewLocation, const FVector& ViewForward, const FVector& AnchorLocation, FVector2D& OutScreenPosition);
}
