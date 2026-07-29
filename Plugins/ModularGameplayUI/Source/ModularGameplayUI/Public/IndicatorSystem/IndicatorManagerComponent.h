// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ControllerComponent.h"

#include "IndicatorManagerComponent.generated.h"

#define UE_API MODULARGAMEPLAYUI_API

class AController;
class UIndicatorDescriptor;
class UObject;
struct FFrame;

/**
 * 指示器管理组件。
 *
 * 挂在 Controller 上，集中维护当前所有活动的屏幕指示器（UIndicatorDescriptor），
 * 并在新增 / 移除时广播事件，供 SActorCanvas 等表现层监听。
 */
UCLASS(MinimalAPI, BlueprintType, Blueprintable)
class UIndicatorManagerComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	UE_API UIndicatorManagerComponent(const FObjectInitializer& ObjectInitializer);

	/** 从指定 Controller 上获取本组件实例。 */
	static UE_API UIndicatorManagerComponent* GetComponent(AController* Controller);

	/** 新增一个指示器。 */
	UFUNCTION(BlueprintCallable, Category = Indicator)
	UE_API void AddIndicator(UIndicatorDescriptor* IndicatorDescriptor);

	/** 移除一个指示器。 */
	UFUNCTION(BlueprintCallable, Category = Indicator)
	UE_API void RemoveIndicator(UIndicatorDescriptor* IndicatorDescriptor);

	DECLARE_EVENT_OneParam(UIndicatorManagerComponent, FIndicatorEvent, UIndicatorDescriptor* Descriptor)
	FIndicatorEvent OnIndicatorAdded;
	FIndicatorEvent OnIndicatorRemoved;

	const TArray<UIndicatorDescriptor*>& GetIndicators() const { return Indicators; }

private:
	UPROPERTY()
	TArray<TObjectPtr<UIndicatorDescriptor>> Indicators;
};

#undef UE_API
