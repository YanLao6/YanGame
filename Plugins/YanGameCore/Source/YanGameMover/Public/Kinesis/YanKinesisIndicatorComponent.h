#pragma once

#include "CoreMinimal.h"
#include "Components/ControllerComponent.h"

#include "YanKinesisIndicatorComponent.generated.h"

#define UE_API YANGAMEMOVER_API

class APlayerController;
class UIndicatorDescriptor;
class UUserWidget;
class UYanKinesisTargetComponent;

/**
 * 念力可控物的屏幕指示器：把够得着的目标标记在玩家屏幕上。
 *
 * 挂在 PlayerController 上，按 ScanInterval 周期扫描 UYanKinesisRegistrySubsystem，
 * 为距离内且投影落在屏幕内的目标登记一个 UIndicatorDescriptor，离开条件时注销。
 * 指示器本身由 IndicatorSystem 的 SActorCanvas 逐帧投影，widget 是屏幕空间子控件，
 * 因而尺寸恒定、不随目标远近缩放——远处的可控物同样看得见、点得着。
 *
 * 扫描而非逐帧刷新：位置的逐帧跟随由 SActorCanvas 承担，本组件只决定「谁该有指示器」，
 * 这个集合的变化远慢于一帧。
 *
 * 只在本地控制端运行：指示器是纯粹的本地表现，专用服务器与远端客户端都无从显示。
 *
 * 前置条件：HUD 布局中须有一个 UIndicatorLayer 控件，否则指示器无处渲染。
 */
UCLASS(MinimalAPI, ClassGroup = (Kinesis), Meta = (BlueprintSpawnableComponent))
class UYanKinesisIndicatorComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	UE_API UYanKinesisIndicatorComponent(const FObjectInitializer& ObjectInitializer);

	/** 显示指示器的距离上限（cm），通常略大于念力的生效距离，好让玩家在够到之前先看见 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Kinesis", Meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxDistance = 1500.f;

	/** 扫描间隔（秒）：决定目标进出视野后指示器出现或消失的延迟 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", Meta = (ClampMin = "0.01"))
	float ScanInterval = 0.2f;

	/**
	 * 屏幕边缘的容差，以视口高度为基准的比例。
	 * 留出余量，目标贴近画面边界时指示器不至于反复闪烁。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", Meta = (ClampMin = "0", ClampMax = "0.5"))
	float ScreenMarginRatio = 0.05f;

	/** 目标未指定自己的指示器 Widget 时使用的默认类 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis")
	TSoftClassPtr<UUserWidget> DefaultIndicatorWidgetClass;

protected:
	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End UActorComponent Interface

private:
	UFUNCTION()
	UE_API void RefreshIndicators();

	// 目标当前是否够条件显示：投影成功且落在含容差的屏幕矩形内
	static UE_API bool IsOnScreen(const APlayerController* OwningController, const FVector& ViewLocation, const FVector& ViewForward, const FVector& AnchorLocation, const FVector2D& ViewportSize, float MarginPixels);

	UE_API void AddIndicatorFor(UYanKinesisTargetComponent* Target);
	UE_API void RemoveAllIndicators();

	// 已登记的指示器，键为其对应的目标组件。两侧都用弱引用：
	// 描述符的强引用由 UIndicatorManagerComponent 持有，目标随 Actor 销毁而失效
	TMap<TWeakObjectPtr<UYanKinesisTargetComponent>, TWeakObjectPtr<UIndicatorDescriptor>> ActiveIndicators;

	FTimerHandle ScanTimer;
};

#undef UE_API
