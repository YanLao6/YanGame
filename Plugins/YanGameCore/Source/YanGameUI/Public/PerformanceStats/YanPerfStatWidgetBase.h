#pragma once

#include "CommonUserWidget.h"
#include "Widgets/SLeafWidget.h"

#include "YanPerfStatWidgetBase.generated.h"

#define UE_API YANGAMEUI_API

enum class EYanDisplayablePerformanceStat : uint8;

class UYanPerformanceStatSubsystem;
class FSampledStatCache;

/**
 * 统计曲线的 Slate 绘制体。
 * 数据来自采样缓冲的只读指针，本体不持有所有权。
 */
class SYanStatGraph : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SYanStatGraph)
			: _DesiredSize(150, 50)
			  , _MaxValueToGraph(33.0)
			  , _LineColor(255, 255, 255, 255)
			  , _BackgroundColor(0, 0, 0, 128)
		{
			_Clipping = EWidgetClipping::ClipToBounds;
		}

		SLATE_ARGUMENT(FVector2D, DesiredSize)
		SLATE_ARGUMENT(double, MaxValueToGraph)
		SLATE_ARGUMENT(FColor, LineColor)
		SLATE_ARGUMENT(FColor, BackgroundColor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	//~Begin SWidget Interface
	virtual int32 OnPaint(const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyClippingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	// 数据每帧都在变，必须标记为易变才能持续重绘
	virtual bool ComputeVolatility() const override { return true; }

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	//~End SWidget Interface

	void SetLineColor(const FColor& InColor) { LineColor = InColor; }
	void SetMaxYValue(const double InValue) { MaxYAxisOfGraph = InValue; }
	void SetBackgroundColor(const FColor& InColor) { BackgroundColor = InColor; }

	void UpdateGraphData(const FSampledStatCache* StatData, const float InScaleFactor)
	{
		GraphData   = StatData;
		ScaleFactor = InScaleFactor;
	}

private:
	void DrawGraphLine(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

	FVector2D DesiredSize = {150.0, 50.0};

	// Y 轴上限，超出的采样会被钳制
	double MaxYAxisOfGraph = 33.0;

	float ScaleFactor = 1.0f;

	FColor LineColor       = FColor(255, 255, 255, 255);
	FColor BackgroundColor = FColor(0, 0, 0, 128);

	// 采样缓冲由性能统计子系统持有，此处仅作只读引用
	const FSampledStatCache* GraphData = nullptr;
};

/**
 * 承载统计曲线的 UMG 控件。
 */
UCLASS(MinimalAPI, meta = (DisableNativeTick))
class UYanPerfStatGraph : public UUserWidget
{
	GENERATED_BODY()

public:
	UE_API UYanPerfStatGraph(const FObjectInitializer& ObjectInitializer);

	UE_API void SetLineColor(const FColor& InColor);
	UE_API void SetMaxYValue(const float InValue);
	UE_API void SetBackgroundColor(const FColor& InValue);
	UE_API void UpdateGraphData(const FSampledStatCache* StatData, const float ScaleFactor);

protected:
	//~Begin UWidget Interface
	UE_API virtual TSharedRef<SWidget> RebuildWidget() override;
	UE_API virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~End UWidget Interface

	TSharedPtr<SYanStatGraph> SlateStatGraph;
};

/**
 * 单项性能统计控件的基类，一个实例对应一项统计。
 * 数值不自动刷新，由蓝图按需调用 FetchStatValue 与 UpdateGraphData。
 */
UCLASS(MinimalAPI, Abstract)
class UYanPerfStatWidgetBase : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** 本控件负责显示的统计项 */
	UFUNCTION(BlueprintPure, Category = "Yan|Performance")
	EYanDisplayablePerformanceStat GetStatToDisplay() const { return StatToDisplay; }

	/** 取该统计的当前值，未经缩放 */
	UFUNCTION(BlueprintPure, Category = "Yan|Performance")
	UE_API double FetchStatValue();

	/** 把最新采样缓冲交给曲线控件 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Performance")
	UE_API void UpdateGraphData(const float ScaleFactor = 1.0f);

protected:
	//~Begin UUserWidget Interface
	UE_API virtual void NativeConstruct() override;
	//~End UUserWidget Interface

	UE_API UYanPerformanceStatSubsystem* GetStatSubsystem();

	// 可选的曲线子控件；只显示文字时无需绑定
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UYanPerfStatGraph> PerfStatGraph;

	UPROPERTY(Transient)
	TObjectPtr<UYanPerformanceStatSubsystem> CachedStatSubsystem;

	UPROPERTY(EditAnywhere, Category = Display)
	FColor GraphLineColor = FColor(255, 255, 255, 255);

	UPROPERTY(EditAnywhere, Category = Display)
	FColor GraphBackgroundColor = FColor(0, 0, 0, 128);

	// 曲线 Y 轴上限
	UPROPERTY(EditAnywhere, Category = Display)
	double GraphMaxYValue = 33.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Display)
	EYanDisplayablePerformanceStat StatToDisplay;
};

#undef UE_API
