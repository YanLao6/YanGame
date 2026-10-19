#include "ContextEffects/YanContextEffectCue.h"

#include "ContextEffects/YanContextEffectsInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanContextEffectCue)

bool UYanContextEffectCue::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (!MyTarget || !EffectTag.IsValid())
	{
		return false;
	}

	FYanContextEffectRequest Request;
	Request.EffectTag = EffectTag;
	Request.AttachPoint = AttachPoint;
	Request.LocationOffset = LocationOffset;
	Request.RotationOffset = RotationOffset;
	Request.Contexts = AdditionalContexts;

	// 附着目标留空：技能不掌握目标的身体布局，由响应方决定挂在哪个组件上
	UYanContextEffectsFunctionLibrary::AppendSurfaceContexts(Parameters.PhysicalMaterial.Get(), Request.Contexts);
	UYanContextEffectsFunctionLibrary::DispatchContextEffect(MyTarget, Request);

	return true;
}
