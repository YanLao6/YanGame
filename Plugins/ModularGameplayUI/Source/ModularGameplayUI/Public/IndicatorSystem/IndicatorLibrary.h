// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "IndicatorLibrary.generated.h"

#define UE_API MODULARGAMEPLAYUI_API

class AController;
class UIndicatorManagerComponent;
class UObject;
struct FFrame;

/** 指示器蓝图函数库：提供获取指示器管理组件的便捷入口。 */
UCLASS(MinimalAPI)
class UIndicatorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UE_API UIndicatorLibrary();

	/** 从指定 Controller 获取指示器管理组件。 */
	UFUNCTION(BlueprintCallable, Category = Indicator)
	static UE_API UIndicatorManagerComponent* GetIndicatorManagerComponent(AController* Controller);
};

#undef UE_API
