#pragma once

#include "CoreMinimal.h"

class APawn;
class UPrimitiveComponent;
class UWorld;

namespace ColorTransfer
{
	/** 与给定形状重叠的角色；胶囊与网格同属 Pawn 通道，结果已按角色去重。 */
	TSet<APawn*> GatherPawnsInShape(const UWorld* World, const FVector& Location, const FQuat& Rotation, const FCollisionShape& Shape);

	/** 与 Volume 形状重叠的角色。 */
	TSet<APawn*> GatherPawnsInVolume(const UPrimitiveComponent* Volume);

	/**
	 * 本地玩家的相机中心射线，只在本地控制端有意义。
	 * @param bStartAtPawn 为 true 时把起点沿射线推到与 Pawn 视点齐平处（同 RangedWeapon 的 CameraTowardsFocus），
	 *                     避免第三人称下射线从角色背后穿过身体
	 * @return 取不到 PlayerController 时返回 false
	 */
	bool GetLocalCameraAim(const APawn* Pawn, bool bStartAtPawn, FVector& OutStart, FVector& OutDirection);

	/** 服务器世界时间，各端一致；GameState 未就绪时退回本地时间。 */
	double GetServerTime(const UWorld* World);
}
