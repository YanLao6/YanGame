#include "Kinesis/YanKinesisIndicatorWidget.h"

#include "IndicatorSystem/IndicatorDescriptor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanKinesisIndicatorWidget)

AActor* UYanKinesisIndicatorWidget::GetIndicatorTarget() const
{
	return IndicatorTarget.Get();
}

void UYanKinesisIndicatorWidget::BindIndicator_Implementation(UIndicatorDescriptor* Indicator)
{
	// 目标由指示器组件写入 DataObject；类型不符说明该 widget 被挂到了别处的指示器上
	AActor* TargetActor = Indicator ? Cast<AActor>(Indicator->GetDataObject()) : nullptr;

	IndicatorTarget = TargetActor;

	OnIndicatorBound(TargetActor);
}

void UYanKinesisIndicatorWidget::UnbindIndicator_Implementation(const UIndicatorDescriptor* Indicator)
{
	IndicatorTarget.Reset();

	OnIndicatorUnbound();
}
