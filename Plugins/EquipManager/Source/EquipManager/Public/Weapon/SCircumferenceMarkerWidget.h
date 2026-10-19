// 圆周标记 Slate 控件声明。

#pragma once

#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Accessibility/SlateWidgetAccessibleTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"
#include "SCircumferenceMarkerWidget.generated.h"

class FPaintArgs;
class FSlateRect;
class FSlateWindowElementList;
class FWidgetStyle;
struct FGeometry;
struct FSlateBrush;

/** 圆周上的一个标记：所处角度与自身的旋转角度。 */
USTRUCT(BlueprintType)
struct FCircumferenceMarkerEntry
{
	GENERATED_BODY()

	/** 标记在圆周上的位置角度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ForceUnits=deg))
	float PositionAngle = 0.0f;

	/** 标记图像自身的旋转角度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ForceUnits=deg))
	float ImageRotationAngle = 0.0f;
};

/**
 * 沿圆周绘制若干标记的 Slate 控件。
 *
 * 准星以此表现散布：半径随散布角变化，四角标记随之扩散或收拢。
 */
class SCircumferenceMarkerWidget : public SLeafWidget
{
	SLATE_BEGIN_ARGS(SCircumferenceMarkerWidget)
		: _MarkerBrush(FCoreStyle::Get().GetBrush("Throbber.CircleChunk"))
		, _Radius(48.0f)
	{
	}
		/** 单个标记的外观。 */
		SLATE_ARGUMENT(const FSlateBrush*, MarkerBrush)
		/** 各标记所处的角度。 */
		SLATE_ARGUMENT(TArray<FCircumferenceMarkerEntry>, MarkerList)
		/** 圆周半径。 */
		SLATE_ATTRIBUTE(float, Radius)
		/** 标记的颜色与不透明度。 */
		SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);

	SCircumferenceMarkerWidget();

	//~Begin SWidget Interface
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual bool ComputeVolatility() const override { return true; }
	//~End SWidget Interface

	void SetRadius(float NewRadius);
	void SetMarkerList(TArray<FCircumferenceMarkerEntry>& NewMarkerList);

private:
	FSlateRenderTransform GetMarkerRenderTransform(const FCircumferenceMarkerEntry& Marker, const float BaseRadius, const float HUDScale) const;

	const FSlateBrush* MarkerBrush;

	TArray<FCircumferenceMarkerEntry> MarkerList;

	TAttribute<float> Radius;

	TAttribute<FSlateColor> ColorAndOpacity;
	bool bColorAndOpacitySet;

	// 标记贴在半径之外还是之内
	uint8 bReticleCornerOutsideSpreadRadius : 1;
};
