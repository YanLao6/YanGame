// 瞄准辅助目标组件实现。

#include "Input/AimAssistTargetComponent.h"

#include "Input/IAimAssistTargetInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AimAssistTargetComponent)

void UAimAssistTargetComponent::GatherTargetOptions(FAimAssistTargetOptions& OutTargetData)
{
	// 未显式指定形状时，取 Owner 身上第一个形状组件作为命中范围
	if (!TargetData.TargetShapeComponent.IsValid())
	{
		if (const AActor* Owner = GetOwner())
		{
			TargetData.TargetShapeComponent = Owner->FindComponentByClass<UShapeComponent>();
		}
	}
	OutTargetData = TargetData;
}
