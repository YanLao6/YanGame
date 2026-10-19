#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MovementModifier.h"

#include "MoverMovementLibrary.generated.h"

#define UE_API YANGAMEMOVER_API

class UMoverComponent;

/** 向当前 Mover 入队移动效果、LayeredMove 与 Modifier。 */
UCLASS(MinimalAPI)
class UMoverMovementLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 排队一个按比例缩放最大速度的 Modifier，返回可用于取消的句柄。
	 *  蓝图与 AS 侧都无法直接传 CustomThunk 通配结构体，通过此函数桥接。
	 *  瞄准、负重一类持续减速须走 Modifier：直接改移动模式上的速度既不复制也会被回滚覆盖。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Modifier")
	static UE_API FMovementModifierHandle QueueMaxSpeedScale(UMoverComponent* MoverComp, float SpeedScale);

	/** 强制切换 MovementMode（零速度叠加 + ForceMovementMode，保持当前速度不变）。
	 *  钩锁激活时用于确保玩家进入 Falling 模式。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Grappling")
	static UE_API void QueueApplyVelocityEffect(UMoverComponent* MoverComp, FName NewModeName);

	/**
	 * 向目标 MoverComponent 施加 FLayeredMove_Launch 击飞效果。
	 * AS 无法直接构造 TSharedPtr<FLayeredMoveBase>，通过此函数桥接。
	 * @param LaunchVelocity    击飞速度向量（世界空间，cm/s）
	 * @param ForceMovementMode 发射前强制切换的移动模式（NAME_None 则不切换）
	 * @param DurationMs        持续时间（毫秒），0 = 单帧冲击
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Launch")
	static UE_API void QueueLaunchMove(UMoverComponent* MoverComp, FVector LaunchVelocity, FName ForceMovementMode, float DurationMs);

	/**
	 * 向目标 MoverComponent 施加 FLayeredMove_LinearVelocity 持续线速度效果。
	 * AS 无法直接构造 TSharedPtr<FLayeredMoveBase>，通过此函数桥接。
	 * @param Velocity          世界空间速度向量（cm/s），持续时间内每帧施加
	 * @param DurationMs        持续时间（毫秒）；0 = 单帧，< 0 = 需手动结束
	 * @param bOverrideVelocity true 覆盖本帧提议速度（屏蔽方向输入/摩擦，直线冲刺）；false 叠加到正常移动
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Launch")
	static UE_API void QueueLinearVelocityMove(UMoverComponent* MoverComp, FVector Velocity, float DurationMs, bool bOverrideVelocity = false);
};

#undef UE_API
