// 圆周标记 Slate 控件实现。

#include "Weapon/SCircumferenceMarkerWidget.h"

#include "Engine/UserInterfaceSettings.h"
#include "Styling/SlateBrush.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SCircumferenceMarkerWidget)

SCircumferenceMarkerWidget::SCircumferenceMarkerWidget()
{
}

void SCircumferenceMarkerWidget::Construct(const FArguments& InArgs)
{
	MarkerBrush = InArgs._MarkerBrush;
	MarkerList = InArgs._MarkerList;
	Radius = InArgs._Radius;
	bColorAndOpacitySet = InArgs._ColorAndOpacity.IsSet();
	ColorAndOpacity = InArgs._ColorAndOpacity;
}

FSlateRenderTransform SCircumferenceMarkerWidget::GetMarkerRenderTransform(const FCircumferenceMarkerEntry& Marker, const float BaseRadius, const float HUDScale) const
{
	float XRadius = BaseRadius;
	float YRadius = BaseRadius;
	if (bReticleCornerOutsideSpreadRadius)
	{
		XRadius += MarkerBrush->ImageSize.X * 0.5f;
		YRadius += MarkerBrush->ImageSize.X * 0.5f;
	}

	const float LocalRotationRadians = FMath::DegreesToRadians(Marker.ImageRotationAngle);
	const float PositionAngleRadians = FMath::DegreesToRadians(Marker.PositionAngle);

	// 先绕自身中心旋转，再平移到圆周上对应角度的位置
	const FSlateRenderTransform RotateAboutOrigin(Concatenate(
		FVector2D(-MarkerBrush->ImageSize.X * 0.5f, -MarkerBrush->ImageSize.Y * 0.5f),
		FQuat2D(LocalRotationRadians),
		FVector2D(MarkerBrush->ImageSize.X * 0.5f, MarkerBrush->ImageSize.Y * 0.5f)));

	return TransformCast<FSlateRenderTransform>(Concatenate(
		RotateAboutOrigin,
		FVector2D(XRadius * FMath::Sin(PositionAngleRadians) * HUDScale, -YRadius * FMath::Cos(PositionAngleRadians) * HUDScale)));
}

int32 SCircumferenceMarkerWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const bool bIsEnabled = ShouldBeEnabled(bParentEnabled);
	const ESlateDrawEffect DrawEffects = bIsEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FVector2D LocalCenter = AllottedGeometry.GetLocalPositionAtCoordinates(FVector2D(0.5f, 0.5f));

	const bool bDrawMarkers = (MarkerList.Num() > 0) && (MarkerBrush != nullptr);

	if (bDrawMarkers)
	{
		const FLinearColor MarkerColor = bColorAndOpacitySet
			? ColorAndOpacity.Get().GetColor(InWidgetStyle)
			: (InWidgetStyle.GetColorAndOpacityTint() * MarkerBrush->GetTint(InWidgetStyle));

		if (MarkerColor.A > KINDA_SMALL_NUMBER)
		{
			const float BaseRadius = Radius.Get();
			const float ApplicationScale = GetDefault<UUserInterfaceSettings>()->ApplicationScale;
			for (const FCircumferenceMarkerEntry& Marker : MarkerList)
			{
				const FSlateRenderTransform MarkerTransform = GetMarkerRenderTransform(Marker, BaseRadius, ApplicationScale);

				const FPaintGeometry Geometry(AllottedGeometry.ToPaintGeometry(
					MarkerBrush->ImageSize,
					FSlateLayoutTransform(LocalCenter - (MarkerBrush->ImageSize * 0.5f)),
					MarkerTransform,
					FVector2D(0.0f, 0.0f)));
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry, MarkerBrush, DrawEffects, MarkerColor);
			}
		}
	}

	return LayerId;
}

FVector2D SCircumferenceMarkerWidget::ComputeDesiredSize(float) const
{
	check(MarkerBrush);
	const float SampledRadius = Radius.Get();
	return FVector2D((MarkerBrush->ImageSize.X + SampledRadius) * 2.0f, (MarkerBrush->ImageSize.Y + SampledRadius) * 2.0f);
}

void SCircumferenceMarkerWidget::SetRadius(float NewRadius)
{
	if (Radius.IsBound() || (Radius.Get() != NewRadius))
	{
		Radius = NewRadius;
		Invalidate(EInvalidateWidgetReason::Layout);
	}
}

void SCircumferenceMarkerWidget::SetMarkerList(TArray<FCircumferenceMarkerEntry>& NewMarkerList)
{
	MarkerList = NewMarkerList;
}
