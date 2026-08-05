#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "IndicatorSystem/IActorIndicatorWidget.h"

#include "YanKinesisIndicatorWidget.generated.h"

#define UE_API YANGAMEUI_API

class UIndicatorDescriptor;

/**
 * 念力可控物的屏幕指示器 Widget 基类。
 *
 * WBP 继承本类即可，位置与显隐由 SActorCanvas 逐帧投影驱动，无须自行跟随目标。
 * 本类只负责把「当前标的是谁」交给蓝图：外观（图标、名称、距离提示）在 WBP 中表达。
 *
 * IIndicatorWidgetInterface 未开放蓝图实现，故绑定的转发必须落在 C++ 侧。
 */
UCLASS(MinimalAPI, Abstract, Blueprintable)
class UYanKinesisIndicatorWidget : public UUserWidget, public IIndicatorWidgetInterface
{
	GENERATED_BODY()

public:
	/** 当前指向的目标 Actor，未绑定时为空 */
	UFUNCTION(BlueprintPure, Category = "Kinesis")
	UE_API AActor* GetIndicatorTarget() const;

protected:
	/** 蓝图实现：绑定到目标时刷新外观 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Kinesis")
	UE_API void OnIndicatorBound(AActor* TargetActor);

	/** 蓝图实现：解绑时收尾，如停止正在播放的动画 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Kinesis")
	UE_API void OnIndicatorUnbound();

	//~Begin IIndicatorWidgetInterface
	UE_API virtual void BindIndicator_Implementation(UIndicatorDescriptor* Indicator) override;
	UE_API virtual void UnbindIndicator_Implementation(const UIndicatorDescriptor* Indicator) override;
	//~End IIndicatorWidgetInterface

private:
	// widget 由 SActorCanvas 复用，解绑后可能再绑到别的目标；弱引用避免留住已销毁的 Actor
	TWeakObjectPtr<AActor> IndicatorTarget;
};

#undef UE_API
