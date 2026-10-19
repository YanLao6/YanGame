#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "MoverQueryLibrary.generated.h"

#define UE_API YANGAMEMOVER_API

class ULayeredMoveLogic;
class UMoverComponent;
class UMoverBlackboard;

/** 查询 Mover 组件、同步输入与仿真黑板中的已有信息；调用方须遵守各接口的线程约束。 */
UCLASS(MinimalAPI)
class UMoverQueryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 从 MoverComponent 已注册的 Logic 列表中找到指定类型的实例（不存在返回 nullptr）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|LayeredMove")
	static UE_API ULayeredMoveLogic* FindRegisteredMoveLogic(UMoverComponent* MoverComp, TSubclassOf<ULayeredMoveLogic> MoveLogicClass);

	/**
	 * 从 MoverBlackboard 读取上一帧地面法线（世界空间 ImpactNormal）。
	 * 无地面数据或地面不可行走时返回 MoverComp 的 UpDirection。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Floor")
	static UE_API FVector GetLastFloorNormal(UMoverComponent* MoverComp);

	/** 黑板中是否存在有效的可行走地面记录（IsWalkableFloor() == true）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Floor")
	static UE_API bool HasWalkableFloor(UMoverComponent* MoverComp);

	/** LayeredMoveLogic 变体：直接接受 SimBlackboard，内部通过 Outer 推导 MoverComponent。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Floor")
	static UE_API bool HasWalkableFloorFromBlackboard(UMoverBlackboard* SimBlackboard);

	/** 从 UMoverBlackboard 的 Outer 推导重力加速度向量。
	 *  ULayeredMoveLogic::GenerateMove 无直接 MoverComp 参数，通过此函数桥接。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Grappling")
	static UE_API FVector GetGravityFromBlackboard(UMoverBlackboard* SimBlackboard);

	/**
	 * 获取 Pawn 拥有者的视点世界坐标与前向量，用于发射/瞄准取位。
	 * 游戏线程原点取 Pawn::GetPawnViewLocation()，方向取 Mover 最近使用的输入 ControlRotation；缺输入时退回身体朝向。
	 * 各端使用相同取位规则，不代表采样时刻或结果相同；不依赖 PlayerCameraManager，不供物理线程或历史回滚判定使用。
	 * Mover 无效或拥有者非 Pawn 时返回 false，Out 参数不写入。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Grappling")
	static UE_API bool GetOwnerViewLocationAndForward(UMoverComponent* MoverComp, FVector& OutLocation, FVector& OutForward);
};

#undef UE_API
