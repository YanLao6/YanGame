// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ModularGameplayUIUtils.generated.h"

#define UE_API MODULARGAMEPLAYUI_API

class APlayerController;
struct FSlateBrush;

/**
 * UI 表现层查询工具：转发脚本与蓝图无法直接访问的 CommonUI 内部状态。
 */
UCLASS(MinimalAPI)
class UModularGameplayUIUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 取按键在当前输入设备下的图标 brush（ControllerData 配置）。
	 * 有图标时回填 OutBrush 并返回 true，供 UI 直接 SetBrush 到 UImage；无图标返回 false，由调用方退回文字提示。
	 * 与 UCommonActionWidget 内部解析不同，此处用显式传入的 PlayerController 定位本地玩家，避免动态生成子控件 owning-player 缺失导致解析失败。
	 */
	UFUNCTION(BlueprintCallable, Category = "ModularGameplayUI|Input")
	static UE_API bool GetKeyBrush(const APlayerController* PlayerController, FKey Key, FSlateBrush& OutBrush);
};

#undef UE_API
