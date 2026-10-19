#include "PerformanceStats/YanPerfStatWidgetBase.h"

#include "Engine/GameInstance.h"
#include "Performance/YanPerformanceStatSubsystem.h"
#include "Styling/CoreStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanPerfStatWidgetBase)

void SYanStatGraph::Construct(const FArguments& InArgs)
{
	DesiredSize     = InArgs._DesiredSize;
	MaxYAxisOfGraph = InArgs._MaxValueToGraph;
	LineColor       = InArgs._LineColor;
	BackgroundColor = InArgs._BackgroundColor;
}

int32 SYanStatGraph::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayerId = LayerId;

	FSlateDrawElement::MakeRotatedBox(
		OutDrawElements,
		MaxLayerId,
		AllottedGeometry.ToPaintGeometry(),
		FCoreStyle::Get().GetBrush("BlackBrush"),
		ESlateDrawEffect::NoPixelSnapping,
		0,
		TOptional<FVector2D>(),
		FSlateDrawElement::RelativeToElement,
		BackgroundColor);

	// 曲线须绘制在背景之上
	++MaxLayerId;
	DrawGraphLine(AllottedGeometry, OutDrawElements, MaxLayerId);

	return ++MaxLayerId;
}

FVector2D SYanStatGraph::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return DesiredSize;
}

void SYanStatGraph::DrawGraphLine(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	if (!GraphData)
	{
		return;
	}

	TArray<FVector2D> Points;
	Points.Reset(GraphData->GetSampleSize() + 1);

	const FVector2D WidgetSize   = AllottedGeometry.GetLocalSize();
	const float     LineThickness = 1.0f;
	const double    XSlice        = WidgetSize.X / static_cast<double>(GraphData->GetSampleSize());
	const double    Border        = 1.0;

	int32 i = 0;
	GraphData->ForEachCurrentSample([&](const double Stat)
	{
		double Y = WidgetSize.Y - FMath::Clamp(Stat * ScaleFactor, 0.0, MaxYAxisOfGraph) / MaxYAxisOfGraph * WidgetSize.Y;
		Y        = FMath::Clamp(Y, Border, WidgetSize.Y - Border);

		Points.Emplace(XSlice * static_cast<double>(++i), Y);
	});

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		Points,
		ESlateDrawEffect::NoPixelSnapping,
		LineColor,
		false,
		LineThickness);
}

UYanPerfStatGraph::UYanPerfStatGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetClipping(EWidgetClipping::ClipToBounds);
}

TSharedRef<SWidget> UYanPerfStatGraph::RebuildWidget()
{
	return SAssignNew(SlateStatGraph, SYanStatGraph);
}

void UYanPerfStatGraph::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	SlateStatGraph.Reset();
}

void UYanPerfStatGraph::SetLineColor(const FColor& InColor)
{
	if (SlateStatGraph.IsValid())
	{
		SlateStatGraph->SetLineColor(InColor);
	}
}

void UYanPerfStatGraph::SetMaxYValue(const float InValue)
{
	if (SlateStatGraph.IsValid())
	{
		SlateStatGraph->SetMaxYValue(InValue);
	}
}

void UYanPerfStatGraph::SetBackgroundColor(const FColor& InValue)
{
	if (SlateStatGraph.IsValid())
	{
		SlateStatGraph->SetBackgroundColor(InValue);
	}
}

void UYanPerfStatGraph::UpdateGraphData(const FSampledStatCache* StatData, const float ScaleFactor)
{
	if (SlateStatGraph.IsValid())
	{
		SlateStatGraph->UpdateGraphData(StatData, ScaleFactor);
	}
}

double UYanPerfStatWidgetBase::FetchStatValue()
{
	if (const UYanPerformanceStatSubsystem* Subsystem = GetStatSubsystem())
	{
		return Subsystem->GetCachedStat(StatToDisplay);
	}

	return 0.0;
}

void UYanPerfStatWidgetBase::UpdateGraphData(const float ScaleFactor)
{
	if (!PerfStatGraph)
	{
		return;
	}

	if (const UYanPerformanceStatSubsystem* Subsystem = GetStatSubsystem())
	{
		if (const FSampledStatCache* GraphData = Subsystem->GetCachedStatData(StatToDisplay))
		{
			PerfStatGraph->UpdateGraphData(GraphData, ScaleFactor);
		}
	}
}

void UYanPerfStatWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	GetStatSubsystem();

	if (PerfStatGraph)
	{
		PerfStatGraph->SetLineColor(GraphLineColor);
		PerfStatGraph->SetMaxYValue(GraphMaxYValue);
		PerfStatGraph->SetBackgroundColor(GraphBackgroundColor);
	}
}

UYanPerformanceStatSubsystem* UYanPerfStatWidgetBase::GetStatSubsystem()
{
	if (CachedStatSubsystem == nullptr)
	{
		if (const UWorld* World = GetWorld())
		{
			if (const UGameInstance* GameInstance = World->GetGameInstance())
			{
				CachedStatSubsystem = GameInstance->GetSubsystem<UYanPerformanceStatSubsystem>();
			}
		}
	}

	return CachedStatSubsystem;
}
